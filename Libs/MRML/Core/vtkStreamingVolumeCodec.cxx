/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkStreamingVolumeCodec.cxx,v $
  Date:      $Date: 2006/01/06 17:56:48 $
  Version:   $Revision: 1.58 $

=========================================================================auto=*/

// MRML includes
#include "vtkStreamingVolumeCodec.h"

// STD includes
#include <deque>

// VTK includes
#include <vtkObjectFactory.h>

//---------------------------------------------------------------------------
vtkStreamingVolumeCodec::vtkStreamingVolumeCodec()
  : LastDecodedFrame(NULL)
  , LastEncodedFrame(NULL)
{
}

//---------------------------------------------------------------------------
vtkStreamingVolumeCodec::~vtkStreamingVolumeCodec()
{
}

//---------------------------------------------------------------------------
bool vtkStreamingVolumeCodec::DecodeFrame(vtkStreamingVolumeFrame* streamingFrame, vtkImageData* outputImageData)
{
  if (!streamingFrame || !outputImageData)
    {
    vtkErrorMacro("Invalid arguments!");
    return false;
    }

  vtkStreamingVolumeFrame* currentFrame = streamingFrame;

  std::deque<vtkStreamingVolumeFrame*> frames;
  frames.push_back(currentFrame);

  // Decode previous frames if the following circumstance are true:
  // - Current frame is not a keyframe
  // - The frame that was previously decoded is not the same as the frame preceding the current one
  while (currentFrame && !currentFrame->IsKeyFrame() &&
         currentFrame->GetPreviousFrame() != this->LastDecodedFrame)
    {
    currentFrame = currentFrame->GetPreviousFrame();
    frames.push_back(currentFrame);
    }

  while (!frames.empty())
    {
    vtkStreamingVolumeFrame* frame = frames.back();
    if (frame)
      {
      // Decode the required frames
      // Only the final frame needs to be saved to the image
      bool saveDecodedImage = frames.size() == 1;
      if (!this->DecodeFrameInternal(frame, outputImageData, saveDecodedImage))
        {
        vtkErrorMacro("Could not decode frame!");
        return false;
        }
      }
    frames.pop_back();
    }

  this->LastDecodedFrame = streamingFrame;
  return true;
}

//---------------------------------------------------------------------------
bool vtkStreamingVolumeCodec::EncodeImageData(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputStreamingFrame, bool forceKeyFrame/*=true*/)
{
  if (!inputImageData || !outputStreamingFrame)
    {
    vtkErrorMacro("Invalid arguments!");
    return false;
    }

  if (!this->EncodeImageDataInternal(inputImageData, outputStreamingFrame, forceKeyFrame))
    {
    vtkErrorMacro("Could not encode frame!");
    return false;
    }

  if (outputStreamingFrame->IsKeyFrame())
    {
    outputStreamingFrame->SetPreviousFrame(NULL);
    }
  else
    {
    outputStreamingFrame->SetPreviousFrame(this->LastEncodedFrame);
    }

  this->LastEncodedFrame = outputStreamingFrame;
  return true;
}

//---------------------------------------------------------------------------
void vtkStreamingVolumeCodec::SetParameters(std::map<std::string, std::string> parameters)
{
  std::map<std::string, std::string>::iterator parameterIt;
  for (parameterIt = parameters.begin(); parameterIt != parameters.end(); ++parameterIt)
    {
    this->SetParameter(parameterIt->first, parameterIt->second);
    }
}

//----------------------------------------------------------------------------
void vtkStreamingVolumeCodec::WriteXML(ostream& of, int nIndent)
{
  of << " codecFourCC=\"" << this->GetFourCC() << "\"";
  std::map<std::string, std::string>::iterator codecParameterIt;
  for (codecParameterIt = this->Parameters.begin(); codecParameterIt != this->Parameters.end(); ++codecParameterIt)
    {
    of << " " << codecParameterIt->first << "=\"" << codecParameterIt->second << "\"";
    }
}

//---------------------------------------------------------------------------
void vtkStreamingVolumeCodec::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "Codec FourCC:\t" << this->GetFourCC() << std::endl;
  std::map<std::string, std::string>::iterator codecParameterIt;
  for (codecParameterIt = this->Parameters.begin(); codecParameterIt != this->Parameters.end(); ++codecParameterIt)
    {
    os << indent << codecParameterIt->first << "=\"" << codecParameterIt->second << "\"";
    }
}
