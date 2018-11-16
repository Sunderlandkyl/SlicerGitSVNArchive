/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

#include "vtkResampleBinaryLabelmapToFractionalLabelmap.h"

// VTK inlcludes
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkObjectFactory.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkDataObject.h>
#include <vtkSmartPointer.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>

// Segmentation includes
#include "vtkFractionalOperations.h"

vtkStandardNewMacro(vtkResampleBinaryLabelmapToFractionalLabelmap);

//----------------------------------------------------------------------------
vtkResampleBinaryLabelmapToFractionalLabelmap::vtkResampleBinaryLabelmapToFractionalLabelmap()
{
  this->OutputScalarType = VTK_UNSIGNED_CHAR;
  this->OversamplingFactor = 6;
  this->OutputMinimumValue = 0.0;
  this->StepSize = 1.0;

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);

  vtkOrientedImageData* output = vtkOrientedImageData::New();
  this->GetExecutive()->SetOutputData(0, output);
  output->ReleaseData();
  output->Delete();
}

//----------------------------------------------------------------------------
vtkResampleBinaryLabelmapToFractionalLabelmap::~vtkResampleBinaryLabelmapToFractionalLabelmap()
{
}

//----------------------------------------------------------------------------
int vtkResampleBinaryLabelmapToFractionalLabelmap::FillOutputPortInformation(
  int, vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkOrientedImageData");
  return 1;
}

//----------------------------------------------------------------------------
int vtkResampleBinaryLabelmapToFractionalLabelmap::FillInputPortInformation(
  int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkOrientedImageData");
  return 1;
}

//----------------------------------------------------------------------------
template <class BinaryImageType>
void ResampleBinaryToFractional( vtkImageData* binaryLabelmap, vtkImageData* fractionalLabelmap, int oversamplingFactor, int outputMinimumValue,
                                 double stepSize, int outputScalarType, BinaryImageType* binaryTypePtr)
{
  switch (fractionalLabelmap->GetScalarType())
    {
    vtkTemplateMacro(
    ResampleBinaryToFractional2(binaryLabelmap, fractionalLabelmap,
                                oversamplingFactor, outputMinimumValue,
                                stepSize, outputScalarType, (BinaryImageType*)NULL, (VTK_TT*)NULL) );
    default:
      std::cout << "error" << std::endl; // TODO: actual error message
      return;
    }
}

//----------------------------------------------------------------------------
template <class BinaryImageType, class FractionalImageType>
void ResampleBinaryToFractional2( vtkImageData* binaryLabelmap, vtkImageData* fractionalLabelmap, int oversamplingFactor, int outputMinimumValue,
                                  double stepSize, int outputScalarType, BinaryImageType* binaryTypePtr, FractionalImageType* fractionalImageTypePtr )
{

  int binaryDimensions[3] = {0,0,0};
  binaryLabelmap->GetDimensions(binaryDimensions);

  int binaryExtent[6] = {0,-1,0,-1,0,-1};
  binaryLabelmap->GetExtent(binaryExtent);

  int fractionalDimensions[3] = {0,0,0};
  fractionalLabelmap->GetDimensions(fractionalDimensions);

  int fractionalExtent[6] = {0,-1,0,-1,0,-1};
  fractionalLabelmap->GetExtent(fractionalExtent);

  fractionalLabelmap->AllocateScalars(outputScalarType, 1);
  FractionalImageType* fractionalLabelmapPtr = (FractionalImageType*)fractionalLabelmap->GetScalarPointerForExtent(fractionalExtent);
  if (!fractionalLabelmapPtr)
    {
    //vtkErrorMacro("ResampleBinaryToFractional: Failed to allocate memory for output labelmap image!");
    return;
    }
  else
    {
    memset(fractionalLabelmapPtr, outputMinimumValue, ((fractionalExtent[1]-fractionalExtent[0]+1) *
                                                       (fractionalExtent[3]-fractionalExtent[2]+1) *
                                                       (fractionalExtent[5]-fractionalExtent[4]+1) *
                                                       fractionalLabelmap->GetScalarSize() * fractionalLabelmap->GetNumberOfScalarComponents()));
    }

  int jDimensionStep = fractionalDimensions[0];
  int kDimensionStep = fractionalDimensions[0] * fractionalDimensions[1];
  BinaryImageType* binaryLabelmapPtr = (BinaryImageType*)binaryLabelmap->GetScalarPointerForExtent(binaryExtent);
  if (!binaryLabelmapPtr)
    {
    //vtkErrorMacro("ResampleBinaryToFractional2: Binary image pointer is NULL!");
    return;
    }

  for (int k = 0; k < binaryDimensions[2]; ++k)
    {
    for (int j = 0; j < binaryDimensions[1]; ++j)
      {
      for (int i = 0; i < binaryDimensions[0]; ++i)
        {
        FractionalImageType* fractionalVoxelPtr = fractionalLabelmapPtr
          + (i / oversamplingFactor)
          + (j / oversamplingFactor) * jDimensionStep
          + (k / oversamplingFactor) * kDimensionStep;

        if ((*binaryLabelmapPtr) > 0)
          {
          (*fractionalVoxelPtr) += stepSize;
          }

        ++binaryLabelmapPtr;
        }
      }
    }

}

//----------------------------------------------------------------------------
int vtkResampleBinaryLabelmapToFractionalLabelmap::RequestData(vtkInformation *vtkNotUsed(request),
                                                               vtkInformationVector **inputVector,
                                                               vtkInformationVector *outputVector)
{
  // Get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // Get the input and ouptut
  vtkImageData *input = vtkImageData::SafeDownCast( inInfo->Get(vtkDataObject::DATA_OBJECT()) );
  if (!input)
  {
    vtkErrorMacro(<< "RequestData: No input");
    return 1;
  }

  vtkImageData *output = vtkImageData::SafeDownCast( outInfo->Get(vtkDataObject::DATA_OBJECT()) );
  if (!output)
  {
    vtkErrorMacro(<< "RequestData: No output");
    return 1;
  }

  vtkSmartPointer<vtkOrientedImageData> binaryLabelmap = vtkSmartPointer<vtkOrientedImageData>::New();
  binaryLabelmap->ShallowCopy(input);

  vtkSmartPointer<vtkOrientedImageData> fractionalLabelmap = vtkSmartPointer<vtkOrientedImageData>::New();
  fractionalLabelmap->CopyDirections(binaryLabelmap);

  double binarySpacing[3] = { 0, 0, 0 };
  binaryLabelmap->GetSpacing(binarySpacing);

  double fractionalSpacing[3] = { binarySpacing[0] * this->OversamplingFactor,
                                  binarySpacing[1] * this->OversamplingFactor,
                                  binarySpacing[2] * this->OversamplingFactor };
  fractionalLabelmap->SetSpacing(fractionalSpacing);

  int binaryDimensions[3] = { 0, 0, 0 };
  binaryLabelmap->GetDimensions(binaryDimensions);

  int binaryExtent[6] = {0, -1, 0, -1, 0, -1};
  binaryLabelmap->GetExtent(binaryExtent);

  int fractionalExtent[6] = { binaryExtent[0]/this->OversamplingFactor, (binaryExtent[1]-this->OversamplingFactor+1)/this->OversamplingFactor,
                              binaryExtent[2]/this->OversamplingFactor, (binaryExtent[3]-this->OversamplingFactor+1)/this->OversamplingFactor,
                              binaryExtent[4]/this->OversamplingFactor, (binaryExtent[5]-this->OversamplingFactor+1)/this->OversamplingFactor };
  fractionalLabelmap->SetExtent(fractionalExtent);

  vtkSmartPointer<vtkMatrix4x4> binaryImageToWorldMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  binaryLabelmap->GetImageToWorldMatrix(binaryImageToWorldMatrix);

  double offset = 0.5*(this->OversamplingFactor-1);
  double ijkOrigin[4] = {offset, offset, offset, 1};
  double* rasOrigin = binaryImageToWorldMatrix->MultiplyDoublePoint(ijkOrigin);

  fractionalLabelmap->SetOrigin(rasOrigin);
  fractionalLabelmap->AllocateScalars(this->OutputScalarType, 1);

  switch (binaryLabelmap->GetScalarType())
  {
    vtkTemplateMacro(
      ResampleBinaryToFractional( binaryLabelmap, fractionalLabelmap,
                                  this->OversamplingFactor, this->OutputMinimumValue,
                                  this->StepSize, this->OutputScalarType, (VTK_TT*)NULL ));

    default:
      vtkErrorMacro(<< "Execute: Unknown scalar type");
      return 1;
  }

  output->ShallowCopy(fractionalLabelmap);
  output->SetExtent(fractionalLabelmap->GetExtent());

  return 1;
}

//----------------------------------------------------------------------------
void vtkResampleBinaryLabelmapToFractionalLabelmap::SetOutput(vtkOrientedImageData* output)
{
    this->GetExecutive()->SetOutputData(0, output);
}

//----------------------------------------------------------------------------
vtkOrientedImageData* vtkResampleBinaryLabelmapToFractionalLabelmap::GetOutput()
{
  if (this->GetNumberOfOutputPorts() < 1)
    {
    return NULL;
    }

  return vtkOrientedImageData::SafeDownCast(
    this->GetExecutive()->GetOutputData(0));
}