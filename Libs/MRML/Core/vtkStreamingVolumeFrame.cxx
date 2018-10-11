/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkStreamingVolumeFrame.cxx,v $
  Date:      $Date: 2006/01/06 17:56:48 $
  Version:   $Revision: 1.58 $

=========================================================================auto=*/

#include "vtkStreamingVolumeFrame.h"
#include <vtkObjectFactory.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkStreamingVolumeFrame);

//---------------------------------------------------------------------------
vtkStreamingVolumeFrame::vtkStreamingVolumeFrame()
  : FrameData(NULL)
  , PreviousFrame(NULL)
  , FrameType(PFrame)
  , NumberOfComponents(3)
{
  this->FrameDimensions[0] = 0;
  this->FrameDimensions[1] = 0;
  this->FrameDimensions[2] = 0;
}

//---------------------------------------------------------------------------
vtkStreamingVolumeFrame::~vtkStreamingVolumeFrame()
{
}

//---------------------------------------------------------------------------
void vtkStreamingVolumeFrame::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << "CodecFourCC: " << CodecFourCC << "\n";
  os << "FrameType: " << this->FrameType << "\n";
  os << "FrameDimensions: [" << this->FrameDimensions[0] << this->FrameDimensions[1] << this->FrameDimensions[2] << "]\n";
  os << "NumberOfComponents: " << this->NumberOfComponents << "\n";
  os << "CurrentFrame: " << this->FrameData << "\n";
  os << "PreviousFrame: " << this->PreviousFrame << "\n";
}
