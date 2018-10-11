/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkStreamingVolumeFrame.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/

#ifndef __vtkStreamingVolumeFrame_h
#define __vtkStreamingVolumeFrame_h

// VTK includes
#include <vtkObject.h>
#include <vtkUnsignedCharArray.h>

// MRML includes
#include "vtkMRML.h"

/// \brief VTK object containing a single compressed frame
class VTK_MRML_EXPORT vtkStreamingVolumeFrame : public vtkObject
{
public:

  enum
  {
    IFrame,
    PFrame,
    BFrame,
  };

  static vtkStreamingVolumeFrame* New();
  vtkTypeMacro(vtkStreamingVolumeFrame, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkSetMacro(FrameType, int);
  vtkGetMacro(FrameType, int);

  void SetFrameData(vtkUnsignedCharArray* frameData) { this->FrameData = frameData; };
  vtkUnsignedCharArray* GetFrameData() { return this->FrameData; };

  void SetPreviousFrame(vtkStreamingVolumeFrame* previousFrame) { this->PreviousFrame = previousFrame; };
  vtkStreamingVolumeFrame* GetPreviousFrame() { return this->PreviousFrame; };

  vtkSetVector3Macro(FrameDimensions, int);
  vtkGetVector3Macro(FrameDimensions, int);

  vtkSetMacro(NumberOfComponents, int);
  vtkGetMacro(NumberOfComponents, int);

  vtkSetMacro(CodecFourCC, std::string);
  vtkGetMacro(CodecFourCC, std::string);

  bool IsKeyFrame() { return this->FrameType == IFrame; };

protected:

  /// Reflects the type of the frame (I-Frame, P-Frame, B-Frame)
  int FrameType;

  /// Dimensions of the decoded frame
  int                                         FrameDimensions[3];

  /// Number of components for the decoded image
  int                                         NumberOfComponents;

  /// Pointer to the contents of the frame in a compressed codec format
  vtkSmartPointer<vtkUnsignedCharArray>       FrameData;

  /// Pointer to the last frame that must be decoded before this one
  /// The pointer of each frame to the previous frame forms a linked list back to the originating keyframe
  /// this ensures that each frame provides access the information neccesary to be able to decode it.
  /// PreviousFrame does not refer to the frame that should be displayed before the this frame,
  /// but the frame that should be decoded immediately before this frame
  vtkSmartPointer<vtkStreamingVolumeFrame>    PreviousFrame;

  /// FourCC of the codec for the frame
  std::string                                 CodecFourCC;

protected:
  vtkStreamingVolumeFrame();
  ~vtkStreamingVolumeFrame();

private:
  vtkStreamingVolumeFrame(const vtkStreamingVolumeFrame&);
  void operator=(const vtkStreamingVolumeFrame&);

};
#endif
