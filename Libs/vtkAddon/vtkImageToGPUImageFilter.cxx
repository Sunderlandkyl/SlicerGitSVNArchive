/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImageToGPUImageFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkImageToGPUImageFilter.h"

#include "vtk_glew.h"
#include "vtkGPUImageData.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTextureObject.h"

#include <sstream>

vtkStandardNewMacro(vtkImageToGPUImageFilter);

//----------------------------------------------------------------------------
vtkImageToGPUImageFilter::vtkImageToGPUImageFilter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->RenderWindow = nullptr;
}

//----------------------------------------------------------------------------
vtkImageToGPUImageFilter::~vtkImageToGPUImageFilter()
{
  this->SetRenderWindow(nullptr);
}

//----------------------------------------------------------------------------
void vtkImageToGPUImageFilter::SetRenderWindow(vtkRenderWindow *renWin)
{
  if (renWin == this->RenderWindow.GetPointer())
  {
    return;
  }

  vtkOpenGLRenderWindow *orw  = nullptr;
  if (renWin)
  {
    orw = vtkOpenGLRenderWindow::SafeDownCast(renWin);
  }

  this->RenderWindow = orw;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkRenderWindow* vtkImageToGPUImageFilter::GetRenderWindow()
{
  return this->RenderWindow;
}

//----------------------------------------------------------------------------
void vtkImageToGPUImageFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "RenderWindow:";
  if(this->RenderWindow != nullptr)
  {
    this->RenderWindow->PrintSelf(os,indent);
  }
  else
  {
    os << "(none)" <<endl;
  }
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::RequestUpdateExtent(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* vtkNotUsed(outputVector))
{
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  int ext[6];
  inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext);
  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), ext, 6);
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkGPUImageData");
  info->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::ProcessRequest(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
  {
    return this->RequestDataObject(request, inputVector, outputVector);
  }

  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
  {
    return this->RequestData(request, inputVector, outputVector);
  }

  // execute information
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
  {
    return this->RequestInformation(request, inputVector, outputVector);
  }

  // propagate update extent
  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  {
    return this->RequestUpdateExtent(request, inputVector, outputVector);
  }

  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::RequestInformation(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  if (outInfo->Has(vtkGPUImageData::CONTEXT_OBJECT()))
  {
    this->SetRenderWindow(vtkOpenGLRenderWindow::SafeDownCast(outInfo->Get(vtkGPUImageData::CONTEXT_OBJECT())));
  }
  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkSmartPointer<vtkRenderWindow>::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->Initialize();
  }
  outInfo->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::RequestDataObject(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkGPUImageData *output = vtkGPUImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!output)
  {
    vtkGPUImageData* newOutput = vtkGPUImageData::New();
    outInfo->Set(vtkDataObject::DATA_OBJECT(), newOutput);
    newOutput->Delete();
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageToGPUImageFilter::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  int outputPort = request->Get(vtkDemandDrivenPipeline::FROM_OUTPUT_PORT());
  if (outputPort == -1)
  {
    outputPort = 0;
  }

  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkSmartPointer<vtkImageData> inputImage = vtkImageData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkInformation *outInfo = outputVector->GetInformationObject(outputPort);
  vtkGPUImageData *outputGPUImage = vtkGPUImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  outputGPUImage->SetContext(this->RenderWindow);
  outputGPUImage->SetExtent(outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT()));

  if (!outputGPUImage->AllocateScalarsFromPointer(inputImage->GetScalarType(), inputImage->GetNumberOfScalarComponents(), inputImage->GetScalarPointer()))
  {
    return 0;
  }
  return 1;
}

//----------------------------------------------------------------------------
vtkImageData* vtkImageToGPUImageFilter::GetInput(int port)
{
  return vtkImageData::SafeDownCast(this->GetInputDataObject(port, 0));
}

//----------------------------------------------------------------------------
void vtkImageToGPUImageFilter::SetOutput(vtkGPUImageData* d)
{
  this->GetExecutive()->SetOutputData(0, d);
}

//----------------------------------------------------------------------------
vtkGPUImageData* vtkImageToGPUImageFilter::GetOutput()
{
  return this->GetOutput(0);
}

//----------------------------------------------------------------------------
vtkGPUImageData* vtkImageToGPUImageFilter::GetOutput(int port)
{
  return vtkGPUImageData::SafeDownCast(this->GetOutputDataObject(port));
}
