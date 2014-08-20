// Teddy0@gmail.com

#include "RollercoasterPluginPrivatePCH.h"
#include "IHeadMountedDisplay.h"

//Spline helper functions
static float ApproxLength(const FInterpCurveVector& SplineInfo, const float Start = 0.0f, const float End = 1.0f, const int32 ApproxSections = 32)
{
	float SplineLength = 0;
	FVector OldPos = SplineInfo.Eval(Start, FVector::ZeroVector);
	for (int32 i = 1; i <= ApproxSections; i++)
	{
		FVector NewPos = SplineInfo.Eval(FMath::Lerp(Start, End, (float)i / (float)ApproxSections), FVector::ZeroVector);
		SplineLength += (NewPos - OldPos).Size();
		OldPos = NewPos;
	}

	return SplineLength;
}

static float GetKeyForDistance(const FInterpCurveVector& SplineInfo, const float Distance, const int32 ApproxSections = 32)
{
	float SplineLength = 0;
	FVector OldPos = SplineInfo.Eval(0.0f, FVector::ZeroVector);
	for (int32 i = 1; i <= ApproxSections; i++)
	{
		FVector NewPos = SplineInfo.Eval((float)i / (float)ApproxSections, FVector::ZeroVector);
		float SectionLength = (NewPos - OldPos).Size();

		if (SplineLength + SectionLength >= Distance)
		{
			float DistanceAlongSection = Distance - SplineLength;
			float SectionDelta = DistanceAlongSection / SectionLength;
			return i / (float)ApproxSections - (1.f - SectionDelta) / (float)ApproxSections;
		}
		SplineLength += SectionLength;
		OldPos = NewPos;
	}

	return 1.0f;
}

//Nasty hack to access protected member SplineInfo inside ULandscapeSplineSegment class. Don't judge me!
FInterpCurveVector& HackAccessSplineInfo(ULandscapeSplineSegment* SplineSegment)
{
	//This ugly code will crash badly if the ULandscapeSplineSegment changes
	BYTE* HackPtr = (BYTE*)SplineSegment;
#if WITH_EDITORONLY_DATA
	HackPtr += STRUCT_OFFSET(ULandscapeSplineSegment, LDMaxDrawDistance) + sizeof(uint32) * 3;
#else
	HackPtr += STRUCT_OFFSET(ULandscapeSplineSegment, Connections[1]) + sizeof(FLandscapeSplineSegmentConnection);
#endif
	return *(FInterpCurveVector*)HackPtr;
}

//Player Controller class that moves along a Landscape Spline
ARollercoasterPlayerController::ARollercoasterPlayerController(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	CurentSegmentIdx = 0;
	CurrentSegmentDelta = 0;
	CurrentSegmentLength = 0;
	CurrentRollerCoasterVelocity = 0.f;
	AddVelocity = 0.f;
	ClimbingSpeed = 10.f;
	FrictionCoefficient = 0.0125f;
	GravityAcceleration = 9.8f;
	Stopped = true;
	Climbing = false;
	CameraHeight = 0.75f;
	ConfigCameraPitch = false;
	BlueprintCameraPitch = false;
	ConfigCameraRoll = false;
	BlueprintCameraRoll = false;
}

void ARollercoasterPlayerController::Possess(APawn* PawnToPossess)
{
	Super::Possess(PawnToPossess);

	//Remove Pitch/Roll limits, so we can do loop-de-loops
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = 0.f;
		PlayerCameraManager->ViewPitchMax = 359.999f;
		PlayerCameraManager->ViewYawMin = 0.f;
		PlayerCameraManager->ViewYawMax = 359.999f;
		PlayerCameraManager->ViewRollMin = 0.f;
		PlayerCameraManager->ViewRollMax = 359.999f;
	}

	if (GetPawn())
	{
		for (TObjectIterator<ULandscapeSplinesComponent> ObjIt; ObjIt; ++ObjIt)
		{
			TrackSplines = *ObjIt;

			ULandscapeSplineControlPoint* ClosestControlPoint = nullptr;

			//Find the controlpoint closest to the player start point
			FVector PlayerStartLocation = GetPawn()->GetActorLocation() - TrackSplines->GetOwner()->GetActorLocation();
			float ClosestDistSq = FLT_MAX;
			for (int32 i = 0; i < TrackSplines->ControlPoints.Num(); i++)
			{
				float DistSq = (TrackSplines->ControlPoints[i]->Location - PlayerStartLocation).SizeSquared();
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestControlPoint = TrackSplines->ControlPoints[i];
				}
			}

			//Build up an ordered list of track segments (the TrackSplines->Segments array can be in random order)
			bool bTrackHasErrors = false;
			OrderedSegments.Empty(TrackSplines->Segments.Num());
			ULandscapeSplineControlPoint* CurrentControlPoint = ClosestControlPoint;
			while (CurrentControlPoint)
			{
				int32 i = 0;
				for (; i < CurrentControlPoint->ConnectedSegments.Num(); i++)
				{
					if (OrderedSegments.Num() == 0 || CurrentControlPoint->ConnectedSegments[i].Segment != OrderedSegments[0])
					{
						OrderedSegments.Insert(CurrentControlPoint->ConnectedSegments[i].Segment, 0);
						check(CurrentControlPoint != CurrentControlPoint->ConnectedSegments[i].GetFarConnection().ControlPoint);
						CurrentControlPoint = CurrentControlPoint->ConnectedSegments[i].GetFarConnection().ControlPoint;
						break;
					}
				}

				//We didn't find another segment to link to, we have an error!
				if (i == CurrentControlPoint->ConnectedSegments.Num())
				{
					bTrackHasErrors = true;
					break;
				}

				//Back to the start
				if (CurrentControlPoint == ClosestControlPoint)
					break;
			}

			//If we found any segments that weren't linked, we have an error
			if (OrderedSegments.Num() != TrackSplines->Segments.Num())
				bTrackHasErrors = true;

			//If there are any errors, bail out!
			if (bTrackHasErrors)
			{
				TrackSplines = nullptr;
				OrderedSegments.Empty();
				continue;
			}

			//Calculate the length of this point
			const FInterpCurveVector& SplineInfo = HackAccessSplineInfo(OrderedSegments[0]);
			CurrentSegmentLength = ApproxLength(SplineInfo);

			break;
		}
	}
}

void ARollercoasterPlayerController::UnPossess()
{
	Super::UnPossess();

	TrackSplines = nullptr;
	OrderedSegments.Empty();
}

/* PlayerTick is only called if the PlayerController has a PlayerInput object.  Therefore, it will not be called on servers for non-locally controlled playercontrollers. */
void ARollercoasterPlayerController::PlayerTick(float DeltaTime)
{
	//If we're requesting a stop, do it immediately
	if (Stopped)
	{
		CurrentRollerCoasterVelocity = 0.f;
	}
	else
	{
		CurrentRollerCoasterVelocity += AddVelocity;
		AddVelocity = 0.f;
	}

	if (GetPawn() && TrackSplines)
	{
		//Cache the world to meters, so we can scale our world up/down
		float WorldToMeters = GetWorldSettings() ? GetWorldSettings()->WorldToMeters : 1.f;

		CurrentSegmentDelta += CurrentRollerCoasterVelocity*DeltaTime;

		//If we're going to pass the next point this frame
		if (CurrentSegmentDelta > CurrentSegmentLength)
		{
			CurentSegmentIdx = CurentSegmentIdx + 1 >= OrderedSegments.Num() ? 0 : CurentSegmentIdx + 1;
			CurrentSegmentDelta = 0.f;

			//Calculate the length of this point
			const FInterpCurveVector& SplineInfo = HackAccessSplineInfo(OrderedSegments[CurentSegmentIdx]);
			CurrentSegmentLength = ApproxLength(SplineInfo);
		}

		ULandscapeSplineSegment* CurrentSegment = OrderedSegments[CurentSegmentIdx];
		const FInterpCurveVector& SplineInfo = HackAccessSplineInfo(CurrentSegment);

		//Do some moving along the track!
		const float NewKeyTime = GetKeyForDistance(SplineInfo, CurrentSegmentDelta);
		const FVector NewKeyPos = TrackSplines->GetOwner()->GetActorLocation() + SplineInfo.Eval(NewKeyTime, FVector::ZeroVector);
		const FVector NewKeyTangent = SplineInfo.EvalDerivative(NewKeyTime, FVector::ZeroVector).SafeNormal();
		FRotator NewRotation = NewKeyTangent.Rotation();

		// Calculate the roll values
		float NewRotationRoll = 0.f;
		if (CurrentSegment->Connections[0].ControlPoint && CurrentSegment->Connections[1].ControlPoint)
		{
			FVector StartLocation; FRotator StartRotation;
			CurrentSegment->Connections[0].ControlPoint->GetConnectionLocationAndRotation(CurrentSegment->Connections[0].SocketName, StartLocation, StartRotation);
			FVector EndLocation; FRotator EndRotation;
			CurrentSegment->Connections[1].ControlPoint->GetConnectionLocationAndRotation(CurrentSegment->Connections[1].SocketName, EndLocation, EndRotation);
			NewRotationRoll = FMath::Lerp(-StartRotation.Roll, -EndRotation.Roll, NewKeyTime);
		}
		NewRotation.Roll = NewRotationRoll;

		//Make the controller/camera face the right direction
		ChairViewRotation = NewRotation;
		if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
		{
			//Remove the Pitch and Roll for VR
			if (!ConfigCameraPitch && !BlueprintCameraRoll)
				ChairViewRotation.Pitch = 0;
			if (!ConfigCameraRoll && !BlueprintCameraRoll)
				ChairViewRotation.Roll = 0;
		}
		SetControlRotation(ChairViewRotation);

		//Make the pawn/chair move along the track
		GetPawn()->SetActorLocation(NewKeyPos);
		GetPawn()->SetActorRotation(NewRotation);

		//Offset the camera so it's above the seat
		CameraOffset = FRotationMatrix(NewRotation).GetScaledAxis(EAxis::Z) * CameraHeight * WorldToMeters;

		//Adjust the velocity of the coaster. Increase acceleration/deceleration when on a slope
		if (!Stopped)
		{
			if (Climbing)
			{
				//Takes 4 seconds to accelerate to climbing speed from rest
				CurrentRollerCoasterVelocity = FMath::Min(ClimbingSpeed, CurrentRollerCoasterVelocity + DeltaTime * (ClimbingSpeed * 0.25f));
			}
			else
			{
				//Add acceleration/deceleration based on the angle of the track
				float Acceleration = FMath::Lerp(0.f, GravityAcceleration, -NewKeyTangent.Z);
				//Add friction on flat level track, less when on an incline
				float Friction = FrictionCoefficient * CurrentRollerCoasterVelocity * (1.f - FMath::Abs(NewKeyTangent.Z));
				CurrentRollerCoasterVelocity += (Acceleration - Friction) * WorldToMeters * DeltaTime;
			}
		}
	}

	PlayerCameraManager->bFollowHmdOrientation = true;

	Super::PlayerTick(DeltaTime);
}

/**
* Updates the rotation of player, based on ControlRotation after RotationInput has been applied.
* This may then be modified by the PlayerCamera, and is passed to Pawn->FaceRotation().
*/
void ARollercoasterPlayerController::UpdateRotation(float DeltaTime)
{
	Super::UpdateRotation(DeltaTime);
}

void ARollercoasterPlayerController::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
	Super::GetPlayerViewPoint(Location, Rotation);
	Location = GetPawn()->GetActorLocation() + CameraOffset;
}