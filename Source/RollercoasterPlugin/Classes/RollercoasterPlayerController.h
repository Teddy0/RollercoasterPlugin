// Teddy0@gmail.com

#pragma once

#include "RollercoasterPlayerController.generated.h"

/**
*
*/
UCLASS()
class ARollercoasterPlayerController : public APlayerController
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FRotator ChairViewRotation;

	UPROPERTY()
	class ULandscapeSplinesComponent* TrackSplines;

	UPROPERTY()
	TArray<class ULandscapeSplineSegment*> OrderedSegments;

	UPROPERTY()
	int32 CurentSegmentIdx;

	UPROPERTY()
	float CurrentSegmentDelta;

	UPROPERTY()
	float CurrentSegmentLength;

	UPROPERTY()
	FVector CameraOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerController)
	float CameraHeight;

	UPROPERTY(BlueprintReadOnly, Category = PlayerController)
	float CurrentRollerCoasterVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerController)
	float RollerCoasterVelocity;

	UPROPERTY(config)
	bool ConfigCameraPitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerController)
	bool BlueprintCameraPitch;

	UPROPERTY(config)
	bool ConfigCameraRoll;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerController)
	bool BlueprintCameraRoll;

public:
	/**
	* Processes player input (immediately after PlayerInput gets ticked) and calls UpdateRotation().
	* PlayerTick is only called if the PlayerController has a PlayerInput object. Therefore, it will only be called for locally controlled PlayerControllers.
	*/
	virtual void PlayerTick(float DeltaTime);

	//Init functions
	virtual void Possess(APawn* PawnToPossess);
	virtual void UnPossess();

	/**
	* Updates the rotation of player, based on ControlRotation after RotationInput has been applied.
	* This may then be modified by the PlayerCamera, and is passed to Pawn->FaceRotation().
	*/
	virtual void UpdateRotation(float DeltaTime) override;

	virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const override;
};