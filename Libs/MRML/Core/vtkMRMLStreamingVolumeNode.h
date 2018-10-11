/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLStreamingVolumeNode.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/

#ifndef __vtkMRMLStreamingVolumeNode_h
#define __vtkMRMLStreamingVolumeNode_h

// MRML includes
#include "vtkMRML.h"
#include "vtkMRMLNode.h"
#include "vtkMRMLStorageNode.h"
#include "vtkMRMLVectorVolumeDisplayNode.h"
#include "vtkMRMLVectorVolumeNode.h"
#include "vtkMRMLVolumeArchetypeStorageNode.h"
#include "vtkStreamingVolumeCodec.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkObject.h>
#include <vtkStdString.h>
#include <vtkUnsignedCharArray.h>

/// \brief MRML node for representing a single compressed video frame
class  VTK_MRML_EXPORT vtkMRMLStreamingVolumeNode : public vtkMRMLVectorVolumeNode
{
public:
  static vtkMRMLStreamingVolumeNode *New();
  vtkTypeMacro(vtkMRMLStreamingVolumeNode,vtkMRMLVectorVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  virtual void ProcessMRMLEvents(vtkObject *caller, unsigned long event, void *callData);

  /// Set node attributes
  virtual void ReadXMLAttributes(const char** atts) VTK_OVERRIDE;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE
  {return "StreamingVolume";}

  virtual void SetAndObserveImageData(vtkImageData* imageData);
  virtual vtkImageData* GetImageData();
  virtual vtkAlgorithmOutput* GetImageDataConnection();

  /// Set and observe the frame object containing the compressed image data
  /// \param frame Object containing the compressed video frame info
  void SetAndObserveFrame(vtkStreamingVolumeFrame* frame);

  /// Returns a pointer to the current frame
  vtkStreamingVolumeFrame* GetFrame(){return this->Frame.GetPointer();};

  /// Encodes the current vtkImageData as a compressed frame using the specified codec
  /// Returns true if the image is successfully encoded
  virtual bool EncodeImageData();

  /// Decodes the current frame and stores the contents in the volume node as vtkImageData
  /// Returns true if the frame is successfully decoded
  virtual bool DecodeFrame();

  /// Returns true if the current frame is a keyframe
  virtual bool IsKeyFrame();

  /// Callback that is called if the current frame is modified
  /// Invokes FrameModifiedEvent
  static void FrameModifiedCallback(vtkObject *caller, unsigned long eid, void* clientData, void* callData);
  enum
  {
    FrameModifiedEvent = 18002
  };

  vtkSetMacro(Codec, vtkStreamingVolumeCodec*);
  vtkStreamingVolumeCodec* GetCodec();

  vtkGetMacro(ImageDataModified, bool);
  vtkSetMacro(ImageDataModified, bool);

  vtkGetMacro(CodecFourCC, std::string);
  vtkSetMacro(CodecFourCC, std::string);

protected:
  vtkMRMLStreamingVolumeNode();
  ~vtkMRMLStreamingVolumeNode();
  vtkMRMLStreamingVolumeNode(const vtkMRMLStreamingVolumeNode&);
  void operator=(const vtkMRMLStreamingVolumeNode&);

  /// Allocates the vtkImageData so that the compressed image data can be decoded
  void AllocateImageForFrame();

  /// Returns true if the number of observers on the ImageData or ImageDataConnection is greater than the default expected number
  bool IsImageObserved();

  vtkGetMacro(ImageAllocationInProgress, bool);
  vtkSetMacro(ImageAllocationInProgress, bool);

  vtkGetMacro(FrameDecoded, bool);
  vtkSetMacro(FrameDecoded, bool);

protected:
  vtkSmartPointer<vtkStreamingVolumeFrame> Frame;
  vtkSmartPointer<vtkStreamingVolumeCodec> Codec;
  std::string                              CodecFourCC;
  bool                                     ImageAllocationInProgress;
  bool                                     FrameDecoded;
  bool                                     ImageDataModified;
  vtkSmartPointer<vtkCallbackCommand>      FrameModifiedCallbackCommand;

};

#endif
