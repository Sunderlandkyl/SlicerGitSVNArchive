/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLStreamingVolumeNode.cxx,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/

#include "vtkMRMLStreamingVolumeNode.h"

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>

// MRML includes
#include <vtkStreamingVolumeCodecFactory.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLStreamingVolumeNode);

const int DEFAULT_NUMBER_OF_IMAGEDATACONNECTION_OBSERVERS = 1;
const int DEFAULT_NUMBER_OF_IMAGEDATA_OBSERVERS = 2;

//----------------------------------------------------------------------------
// vtkMRMLStreamingVolumeNode methods

//-----------------------------------------------------------------------------
vtkMRMLStreamingVolumeNode::vtkMRMLStreamingVolumeNode()
  : Codec(NULL)
  , Frame(NULL)
  , ImageAllocationInProgress(false)
  , ImageDataModified(false)
  , FrameDecoded(false)
  , FrameModifiedCallbackCommand(vtkSmartPointer<vtkCallbackCommand>::New())
{
  this->FrameModifiedCallbackCommand->SetClientData(reinterpret_cast<void *>(this));
  this->FrameModifiedCallbackCommand->SetCallback(vtkMRMLStreamingVolumeNode::FrameModifiedCallback);
}

//-----------------------------------------------------------------------------
vtkMRMLStreamingVolumeNode::~vtkMRMLStreamingVolumeNode()
{
}

//---------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::FrameModifiedCallback(vtkObject *caller, unsigned long eid, void* clientData, void* callData)
{
  vtkMRMLStreamingVolumeNode* self = reinterpret_cast<vtkMRMLStreamingVolumeNode*>(clientData);
  if (!self)
    {
    return;
    }

  if (vtkStreamingVolumeFrame::SafeDownCast(caller) == self->Frame)
    {
    if (self->IsImageObserved())
      {
      self->DecodeFrame();
      }
    self->InvokeCustomModifiedEvent(vtkMRMLStreamingVolumeNode::FrameModifiedEvent);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::ProcessMRMLEvents(vtkObject *caller,
  unsigned long event,
  void *callData)
{
  Superclass::ProcessMRMLEvents(caller, event, callData);
}

//---------------------------------------------------------------------------
bool vtkMRMLStreamingVolumeNode::IsKeyFrame()
{
  if (this->Frame)
    {
    return this->Frame->IsKeyFrame();
    }
  return false;
}

//---------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::SetAndObserveImageData(vtkImageData* imageData)
{
  this->ImageAllocationInProgress = true;
  if (imageData)
    {
    this->ImageDataModified = true;
    }
  else
    {
    this->ImageDataModified = false;
    }
  Superclass::SetAndObserveImageData(imageData);
  this->ImageAllocationInProgress = false;
}

//---------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::AllocateImageForFrame()
{
  if (!this->ImageAllocationInProgress && !this->ImageDataModified)
    {
    this->ImageAllocationInProgress = true;
    vtkSmartPointer<vtkImageData> imageData = Superclass::GetImageData();
    if (!imageData)
      {
      if (this->Frame)
        {
        vtkSmartPointer<vtkImageData> newImageData = vtkSmartPointer<vtkImageData>::New();
        this->SetAndObserveImageData(newImageData);
        imageData = newImageData;
        this->FrameDecoded = false;
        }
      }

    if (imageData && this->Frame)
      {
      int frameDimensions[3] = { 0,0,0 };
      this->Frame->GetFrameDimensions(frameDimensions);
      imageData->SetDimensions(frameDimensions);
      imageData->AllocateScalars(VTK_UNSIGNED_CHAR, this->Frame->GetNumberOfComponents());
      }
    this->ImageDataModified = false;
    this->ImageAllocationInProgress = false;
    }
}

//---------------------------------------------------------------------------
vtkImageData* vtkMRMLStreamingVolumeNode::GetImageData()
{
  if (!this->ImageDataModified && this->Frame)
    {
    this->DecodeFrame();
    }

  vtkImageData* imageData = Superclass::GetImageData();
  return imageData;
}

//---------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLStreamingVolumeNode::GetImageDataConnection()
{
  if (!this->ImageAllocationInProgress && !this->ImageDataModified)
    {
    this->DecodeFrame();
    }
  return Superclass::GetImageDataConnection();
}

//---------------------------------------------------------------------------
vtkStreamingVolumeCodec* vtkMRMLStreamingVolumeNode::GetCodec()
{
  if (!this->Codec ||
      (this->Codec &&
       this->Codec->GetFourCC() != this->GetCodecFourCC()))
    {
    this->Codec = vtkStreamingVolumeCodecFactory::GetInstance()->CreateCodecByFourCC(this->GetCodecFourCC());
    }
  return this->Codec;
}

//---------------------------------------------------------------------------
bool vtkMRMLStreamingVolumeNode::IsImageObserved()
{
  vtkImageData* imageData = Superclass::GetImageData();
  if ((this->ImageDataConnection != NULL &&
       this->ImageDataConnection->GetReferenceCount() > DEFAULT_NUMBER_OF_IMAGEDATACONNECTION_OBSERVERS) ||
      (imageData && imageData->GetReferenceCount() > DEFAULT_NUMBER_OF_IMAGEDATA_OBSERVERS))
    {
    return true;
    }
  return false;
}

//---------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::SetAndObserveFrame(vtkStreamingVolumeFrame* frame)
{
  if (this->Frame == frame)
    {
    return;
    }

  if (this->Frame)
    {
    this->Frame->RemoveObservers(vtkCommand::ModifiedEvent, this->FrameModifiedCallbackCommand);
    }

  this->Frame = frame;
  this->FrameDecoded = false;

  if (this->Frame)
    {
    this->Frame->AddObserver(vtkCommand::ModifiedEvent, this->FrameModifiedCallbackCommand);
    }

  if (this->Frame && !this->ImageDataModified)
    {
    this->CodecFourCC = this->Frame->GetCodecFourCC();

    // If the image is being observed beyond the default internal observations of the volume node, then the frame should be decoded
    // since some external class is observing the image data.
    if (this->IsImageObserved())
      {
      this->DecodeFrame();
      }
    }
  this->Modified();
}

//---------------------------------------------------------------------------
bool vtkMRMLStreamingVolumeNode::DecodeFrame()
{
  if (!this->Frame)
    {
    vtkErrorMacro("No frame to decode!");
    return false;
    }

  if (this->FrameDecoded)
    {
    // Frame is already decoded.
    // Doesn't need to be decoded twice.
    return true;
    }

  this->AllocateImageForFrame();
  vtkImageData* imageData = Superclass::GetImageData();
  if (!imageData)
    {
    vtkErrorMacro("Cannot decode frame. No destination image data!");
    return false;
    }

  if (!this->GetCodec())
    {
    vtkErrorMacro("Could not find codec \"" << this->GetCodecFourCC() << "\"");
    return false;
    }

  if (!this->Codec->DecodeFrame(this->Frame, imageData))
    {
    vtkErrorMacro("Could not decode frame");
    return false;
    }

  this->FrameDecoded = true;
  this->ImageDataModified = false;
  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLStreamingVolumeNode::EncodeImageData()
{
  vtkImageData* imageData = Superclass::GetImageData();
  if (!imageData)
    {
    vtkErrorMacro("No image data to encode!")
    return false;
    }

  if (!this->GetCodec())
    {
    vtkErrorMacro("Could not find codec \"" << this->GetCodecFourCC() << "\"");
    return false;
    }

  if (!this->Frame)
    {
    this->Frame = vtkSmartPointer<vtkStreamingVolumeFrame>::New();
    }

  if (!this->Codec->EncodeImageData(this->GetImageData(), this->Frame))
    {
    vtkErrorMacro("Could not encode frame!");
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLBooleanMacro(imageAllocationInProgress, ImageAllocationInProgress);
  vtkMRMLWriteXMLBooleanMacro(frameDecoded, FrameDecoded);
  vtkMRMLWriteXMLBooleanMacro(imageDataModified, ImageDataModified);
  if (this->Codec)
    {
    this->Codec->WriteXML(of, nIndent);
    }
  else
    {
    vtkMRMLWriteXMLStdStringMacro(codecFourCC, CodecFourCC);
    }
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();
  Superclass::ReadXMLAttributes(atts);
  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLStdStringMacro(codecFourCC, CodecFourCC);
  vtkMRMLReadXMLBooleanMacro(imageAllocationInProgress, ImageAllocationInProgress);
  vtkMRMLReadXMLBooleanMacro(frameDecoded, FrameDecoded);
  vtkMRMLReadXMLBooleanMacro(imageDataModified, ImageDataModified);
  vtkMRMLReadXMLEndMacro();
  if (this->GetCodec())
    {
    this->Codec->ReadXMLAttributes(atts);
    }
  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  Superclass::Copy(anode);
  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyStdStringMacro(CodecFourCC);
  vtkMRMLCopyBooleanMacro(ImageAllocationInProgress);
  vtkMRMLCopyBooleanMacro(FrameDecoded);
  vtkMRMLCopyBooleanMacro(ImageDataModified);
  this->SetAndObserveFrame(this->SafeDownCast(copySourceNode)->GetFrame());
  vtkMRMLCopyEndMacro();
  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLStreamingVolumeNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintBooleanMacro(ImageAllocationInProgress);
  vtkMRMLPrintBooleanMacro(FrameDecoded);
  vtkMRMLPrintBooleanMacro(ImageDataModified);
  os << indent << this->Frame << "\n";
  if (this->Codec)
    {
    os << indent << this->Codec << "\n";
    }
  else
    {
    vtkMRMLPrintStdStringMacro(CodecFourCC);
    }
  vtkMRMLPrintEndMacro();
}
