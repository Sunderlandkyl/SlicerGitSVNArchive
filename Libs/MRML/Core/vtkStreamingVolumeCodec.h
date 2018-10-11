/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkStreamingVolumeCodec.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/

#ifndef __vtkStreamingVolumeCodec_h
#define __vtkStreamingVolumeCodec_h

// MRML includes
#include "vtkMRML.h"
#include "vtkStreamingVolumeFrame.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkObject.h>
#include <vtkUnsignedCharArray.h>

// STD includes
#include <map>

#ifndef vtkMRMLCodecNewMacro
#define vtkMRMLCodecNewMacro(newClass) \
vtkStandardNewMacro(newClass); \
vtkStreamingVolumeCodec* newClass::CreateCodecInstance() \
{ \
return newClass::New(); \
}
#endif

/// \brief VTK object for representing a volume compression codec (normally a video compression codec)
/// Three functions from this class need to be implemented in child classes:
/// 1. virtual bool EncodeImageDataInternal(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputFrame, bool forceKeyFrame);
/// 2. virtual bool DecodeFrameInternal(vtkStreamingVolumeFrame* inputFrame, vtkImageData* outputImageData, bool saveDecodedImage);
/// 3. virtual std::string GetFourCC();
/// Optionally:
/// 4. virtual bool SetParameter(std::string parameterName, std::string parameterValue);
class VTK_MRML_EXPORT vtkStreamingVolumeCodec : public vtkObject
{
public:
  vtkTypeMacro(vtkStreamingVolumeCodec, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /// Returns the FourCC code representing the codec
  /// See https://www.fourcc.org/codecs.php for an incomplete list
  virtual std::string GetFourCC() { return ""; };

  /// Creates an instance of the codec
  virtual vtkStreamingVolumeCodec* CreateCodecInstance() { return NULL; };

  /// Decode compressed frame data and stores it in the imagedata
  /// Handles the decoding of additional previous frames if required
  /// \param frame Input frame containing the compressed frame data
  /// \param outputImageData Output image which will store the uncompressed image
  /// Returns true if the frame is decoded successfully
  virtual bool DecodeFrame(vtkStreamingVolumeFrame* frame, vtkImageData* outputImageData);

  /// Encode the image data and store it in the frame
  /// \param inputImageData Input image containing the uncompressed image
  /// \param outputStreamingFrame Output frame that will be used to store the compressed frame
  /// \param forceKeyFrame If the codec supports it, attempt to encode the image as a keyframe
  /// Returns true if the image is encoded successfully
  virtual bool EncodeImageData(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputStreamingFrame, bool forceKeyFrame = true);

  /// Read this codec's information in XML format
  virtual void ReadXMLAttributes(const char** atts) {};

  /// Write this codec's information in XML format.
  virtual void WriteXML(ostream& of, int indent);

  const std::string CodecParameterPrefix = "Codec.";

  /// Set the parameters for the codec
  /// \param parameterName String containing the name of the parameter in the form "Codec.ParameterName"
  /// \param parameterValue Value of the specified parameter
  /// Returns true if the parameter is successfully set
  virtual bool SetParameter(std::string parameterName, std::string parameterValue) { return false; };

  /// Sets all of the specified parameters in the codec
  /// \parameters Map containing the parameters and values to be set
  virtual void SetParameters(std::map<std::string, std::string> parameters);

protected:

  /// Decode a frame and store its contents in a vtkImageData
  /// This function performs the actual decoding for a single frame and should be implemented in all non abstract subclasses
  /// \param inputFame Frame object containing the compressed data to be decoded
  /// \param outputImageData Image data object that will be used to store the output image
  /// \param saveDecodedImage If true, writes the decoded image to the frame. If false, the decoded results are discarded
  /// Returns true if the frame is decoded successfully
  virtual bool DecodeFrameInternal(vtkStreamingVolumeFrame* inputFrame, vtkImageData* outputImageData, bool saveDecodedImage = true) { return false; };

  /// Decode a vtkImageData and store its contents in a frame
  /// This function performs the actual encoding for a single frame and should be implemented in all non abstract subclasses
  /// \param inputImageData Image data object containing the uncompressed data to be encoded
  /// \param outputFrame Frame object that will be used to store the compressed data
  /// \param forceKeyFrame When true, attempt to encode the image as a keyframe if the codec supports it
  /// Returns true if the image is encoded successfully
  virtual bool EncodeImageDataInternal(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputFrame, bool forceKeyFrame) { return false; };

protected:
  vtkStreamingVolumeCodec();
  ~vtkStreamingVolumeCodec();

private:
  vtkStreamingVolumeCodec(const vtkStreamingVolumeCodec&);
  void operator=(const vtkStreamingVolumeCodec&);

protected:
  vtkSmartPointer<vtkStreamingVolumeFrame>     LastDecodedFrame;
  vtkSmartPointer<vtkStreamingVolumeFrame>     LastEncodedFrame;
  std::map<std::string, std::string>           Parameters;
};

#endif
