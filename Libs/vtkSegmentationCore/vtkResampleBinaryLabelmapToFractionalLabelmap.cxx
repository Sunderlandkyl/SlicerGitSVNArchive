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

#include <vtkFractionalLogicalOperations.h>
#include <vtkOrientedImageData.h>

vtkStandardNewMacro(vtkResampleBinaryLabelmapToFractionalLabelmap);

//----------------------------------------------------------------------------
vtkResampleBinaryLabelmapToFractionalLabelmap::vtkResampleBinaryLabelmapToFractionalLabelmap()
{
  this->OutputScalarType = VTK_UNSIGNED_CHAR;
  this->OversamplingFactor = 6;
  this->OutputMinimumValue = 0.0;
  this->StepSize = 1.0;

  for (int i = 0; i < 5; i += 2)
    {
    this->OutputExtent[i] = 0;
    this->OutputExtent[i+1] = -1;
    }

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

//----------------------------------------------------------------------------
vtkResampleBinaryLabelmapToFractionalLabelmap::~vtkResampleBinaryLabelmapToFractionalLabelmap()
{
}

//----------------------------------------------------------------------------
void vtkResampleBinaryLabelmapToFractionalLabelmap::SetOutputExtent(int extent[6])
{
  for (int i = 0; i < 6; ++i)
  {
    this->OutputExtent[i] = extent[i];
  }
}

//----------------------------------------------------------------------------
void vtkResampleBinaryLabelmapToFractionalLabelmap::SetOutputScalarType(int outputScalarType)
{
  this->OutputScalarType = outputScalarType;
}

//----------------------------------------------------------------------------
void vtkResampleBinaryLabelmapToFractionalLabelmap::SetOutputMinimumValue(double outputMinimumValue)
{
  this->OutputMinimumValue = outputMinimumValue;
}

//----------------------------------------------------------------------------
void vtkResampleBinaryLabelmapToFractionalLabelmap::SetStepSize(double stepSize)
{
  this->StepSize = stepSize;
}

//----------------------------------------------------------------------------
template <class BinaryImageType>
void ResampleBinaryToFractional( vtkImageData* binaryLabelmap, vtkImageData* fractionalLabelmap, int oversamplingFactor, int outputMinimumValue, double stepSize, int outputScalarType, BinaryImageType* binaryTypePtr)
{
  switch (fractionalLabelmap->GetScalarType())
    {
    vtkTemplateMacro( ResampleBinaryToFractional2(binaryLabelmap, fractionalLabelmap, oversamplingFactor, outputMinimumValue, stepSize, outputScalarType, (BinaryImageType*) NULL, (VTK_TT*) NULL));
    default:
      std::cout << "error" << std::endl; // TODO: actual error message
      return;
    }
}

//----------------------------------------------------------------------------
template <class BinaryImageType, class FractionalImageType>
void ResampleBinaryToFractional2( vtkImageData* binaryLabelmap, vtkImageData* fractionalLabelmap, int oversamplingFactor, int outputMinimumValue, double stepSize, int outputScalarType, BinaryImageType* binaryTypePtr, FractionalImageType* fractionalImageTypePtr )
{

  int binaryDimensions[3] = { 0, 0, 0 };
  binaryLabelmap->GetDimensions(binaryDimensions);

  int binaryExtent[6] = { 0, -1, 0, -1, 0, -1 };
  binaryLabelmap->GetExtent(binaryExtent);

  int fractionalDimensions[3] = { 0, 0, 0 };
  fractionalLabelmap->GetDimensions(fractionalDimensions);

  int fractionalExtent[6] = { 0, -1, 0, -1, 0, -1 };
  fractionalLabelmap->GetExtent(fractionalExtent);

  fractionalLabelmap->AllocateScalars(outputScalarType, 1);
  FractionalImageType* fractionalLabelmapPtr = (FractionalImageType*)fractionalLabelmap->GetScalarPointerForExtent(fractionalExtent);
  if (!fractionalLabelmapPtr)
    {
    //vtkErrorMacro("Convert: Failed to allocate memory for output labelmap image!"); TODO
    return;
    }
  else
    {
    memset(fractionalLabelmapPtr, outputMinimumValue, ((fractionalExtent[1]-fractionalExtent[0]+1) *
                                                       (fractionalExtent[3]-fractionalExtent[2]+1) *
                                                       (fractionalExtent[5]-fractionalExtent[4]+1) *
                                                       fractionalLabelmap->GetScalarSize() * fractionalLabelmap->GetNumberOfScalarComponents()));
    }

  BinaryImageType* binaryLabelmapPtr = (BinaryImageType*)binaryLabelmap->GetScalarPointerForExtent(binaryExtent);
  for (int k = 0; k < binaryDimensions[2]; ++k)
    {
    for (int j = 0; j < binaryDimensions[1]; ++j)
      {
      for (int i = 0; i < binaryDimensions[0]; ++i)
        {
        FractionalImageType* fractionalVoxelPtr = fractionalLabelmapPtr
          + (i / oversamplingFactor)
          + (j / oversamplingFactor) * fractionalDimensions[0]
          + (k / oversamplingFactor) * fractionalDimensions[0] * fractionalDimensions[1];

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

  vtkSmartPointer<vtkImageData> binaryLabelmap = vtkSmartPointer<vtkImageData>::New();
  binaryLabelmap->ShallowCopy(input);

  vtkSmartPointer<vtkImageData> fractionalLabelmap = vtkSmartPointer<vtkImageData>::New();

  double binarySpacing[3] = { 0, 0, 0 };
  binaryLabelmap->GetSpacing(binarySpacing);

  double fractionalSpacing[3] = { binarySpacing[0] * this->OversamplingFactor,
                                  binarySpacing[1] * this->OversamplingFactor,
                                  binarySpacing[2] * this->OversamplingFactor };
  fractionalLabelmap->SetSpacing(fractionalSpacing);

  double binaryOrigin[3] = {0,0,0};
  binaryLabelmap->GetOrigin(binaryOrigin);

  int binaryExtent[6] = {0, -1, 0, -1, 0, -1};
  binaryLabelmap->GetExtent(binaryExtent);

  int binaryDimensions[3] = { 0, 0, 0 };
  binaryLabelmap->GetDimensions(binaryDimensions);

  if (this->OutputExtent[0] > this->OutputExtent[1] ||
      this->OutputExtent[2] > this->OutputExtent[3] ||
      this->OutputExtent[4] > this->OutputExtent[5])
    {
    int fractionalDimensions[3] = { std::ceil(binaryDimensions[0] / this->OversamplingFactor),
                                    std::ceil(binaryDimensions[1] / this->OversamplingFactor),
                                    std::ceil(binaryDimensions[2] / this->OversamplingFactor) };
    fractionalLabelmap->SetDimensions(fractionalDimensions);
    }
  else
    {
    fractionalLabelmap->SetExtent(this->OutputExtent);

    int fractionalDimensions[3] = {0,0,0};
    fractionalLabelmap->GetDimensions(fractionalDimensions);

    for (int i = 0; i < 3; ++i)
      {
      if (fractionalDimensions[i] < std::ceil(binaryDimensions[i] / this->OversamplingFactor))
        {
        this->OutputExtent[i+1] = this->OutputExtent[i] + std::ceil(binaryDimensions[i] / this->OversamplingFactor) - 1;
        }
      }

      fractionalLabelmap->SetExtent(this->OutputExtent);
    }

  int fractionalExtent[6] = {0, -1, 0, -1, 0, -1};
  fractionalLabelmap->GetExtent(fractionalExtent);

  double offset = (this->OversamplingFactor - 1.0) / (2.0 * this->OversamplingFactor);
  double fractionalOrigin[3] = { binaryOrigin[0] + offset * fractionalSpacing[0] - fractionalExtent[0] * fractionalSpacing[0],
                                 binaryOrigin[1] + offset * fractionalSpacing[1] - fractionalExtent[2] * fractionalSpacing[1],
                                 binaryOrigin[2] + offset * fractionalSpacing[2] - fractionalExtent[4] * fractionalSpacing[2] };
  fractionalLabelmap->SetOrigin(fractionalOrigin);
  fractionalLabelmap->AllocateScalars(this->OutputScalarType, 1);

  switch (binaryLabelmap->GetScalarType())
  {
    vtkTemplateMacro(ResampleBinaryToFractional( binaryLabelmap, fractionalLabelmap, this->OversamplingFactor, this->OutputMinimumValue, this->StepSize, this->OutputScalarType, (VTK_TT*) NULL));

    default:
      vtkErrorMacro(<< "Execute: Unknown scalar type");
      return 1;
  }

  output->ShallowCopy(fractionalLabelmap);
  output->SetExtent(fractionalLabelmap->GetExtent());

  vtkNew<vtkOrientedImageData> a;
  a->DeepCopy(input);

  vtkFractionalLogicalOperations::Write(a.GetPointer(), "E:\\test\\input.nrrd");
    a->DeepCopy(output);
  vtkFractionalLogicalOperations::Write(a.GetPointer(), "E:\\test\\output.nrrd");

  return 1;
}