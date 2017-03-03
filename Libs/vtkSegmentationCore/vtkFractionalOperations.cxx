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

#include "vtkFractionalOperations.h"

// VTK inlcludes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkFieldData.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkStringArray.h>
#include <vtkImageConstantPad.h>
#include <vtkImageMathematics.h>

// Segmentation includes
#include <vtkOrientedImageData.h>
#include <vtkSegmentationConverter.h>
#include <vtkSegmentation.h>
#include <vtkOrientedImageDataResample.h>

#include <vtkNRRDWriter.h>

vtkStandardNewMacro(vtkFractionalOperations);

const double vtkFractionalOperations::DefaultScalarRange[2] = {-108, 108};
const double vtkFractionalOperations::DefaultThreshold = 0.0;
const vtkIdType vtkFractionalOperations::DefaultInterpolationType = VTK_LINEAR_INTERPOLATION;
const vtkIdType vtkFractionalOperations::DefaultScalarType = VTK_CHAR;

//----------------------------------------------------------------------------
vtkFractionalOperations::vtkFractionalOperations()
{
}

//----------------------------------------------------------------------------
vtkFractionalOperations::~vtkFractionalOperations()
{
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::Invert(vtkOrientedImageData* labelmap)
{
  if (!labelmap)
    {
    std::cerr << "Invert: invalid labelmap" << std::endl;
    return;
    }


  int dimensions[3] = {0,0,0};
  labelmap->GetDimensions(dimensions);

  int extent[6] = {0, -1, 0, -1, 0, -1};
  labelmap->GetExtent(extent);

  if (extent[0] > extent[1] ||
      extent[2] > extent[3] ||
      extent[4] > extent[5])
    {
    std::cerr << "Invert: invalid extent" << std::endl;
    //vtkErrorMacro("Invert: invalid extent"); // TODO: static error macros?
    return;
    }

  double scalarRange[2] = {0.0, 1.0};
  //vtkFractionalOperations::GetScalarRange(labelmap, scalarRange); //TODO
  vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
  labelmap->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
    {
    scalarRange[0] = scalarRangeArray->GetValue(0);
    scalarRange[1] = scalarRangeArray->GetValue(1);
    }

  void* labelmapPointer = labelmap->GetScalarPointerForExtent(extent);

  switch(labelmap->GetScalarType())
    {
    vtkTemplateMacro(InvertGeneric((VTK_TT*)labelmapPointer, dimensions, scalarRange));

    default:
      std::cerr << "Invert: invalid scalar type" << std::endl;
      //vtkErrorMacro("Invert: invalid extent"); // TODO: static error macros? TODO actual message
    }

}

//----------------------------------------------------------------------------
template <class LabelmapScalarType>
void vtkFractionalOperations::InvertGeneric(LabelmapScalarType* labelmapPointer, int dimensions[3], double scalarRange[2])
{
  if (!labelmapPointer)
  {
    std::cerr << "InvertGeneric: invalid labelmap pointer" << std::endl;
    return;
  }

  int numberOfVoxels = dimensions[0]*dimensions[1]*dimensions[2];

  double min = scalarRange[0];
  double max = scalarRange[1];

  for (int i = 0; i < numberOfVoxels; ++i)
    {
    double invertedValue = max - (*labelmapPointer) + min;
    *labelmapPointer = invertedValue;
    ++labelmapPointer;
    }

}

void vtkFractionalOperations::ConvertFractionalImage(vtkOrientedImageData* input, vtkOrientedImageData* output, vtkSegmentation* segmentationTemplate)
{
  vtkOrientedImageData* templateLablemap = vtkOrientedImageData::SafeDownCast(
    segmentationTemplate->GetNthSegment(0)->GetRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));

  if (!templateLablemap || !vtkFractionalOperations::ContainsFractionalParameters(templateLablemap))
    {
    // Voxels do not need to be recalculated
    if (input != output)
      {
      output->DeepCopy(input);
      }
    return;
    }

  vtkFractionalOperations::ConvertFractionalImage(input, output, templateLablemap);
}

void vtkFractionalOperations::ConvertFractionalImage(vtkOrientedImageData* input, vtkOrientedImageData* output, vtkOrientedImageData* outputTemplate)
{
  if (!input || !output || !outputTemplate)
    {
    std::cerr << "ConvertFractionalImage: invalid vtkOrientedImageData!" << std::endl;
    return;
    }

  double inputScalarRange[2] = {vtkFractionalOperations::DefaultScalarRange[0], vtkFractionalOperations::DefaultScalarRange[1]};
  vtkFractionalOperations::GetScalarRange(input, inputScalarRange);
  double outputScalarRange[2] = {vtkFractionalOperations::DefaultScalarRange[0], vtkFractionalOperations::DefaultScalarRange[1]};
  vtkFractionalOperations::GetScalarRange(outputTemplate, inputScalarRange);

  if (input->GetScalarType() == outputTemplate->GetScalarType() &&
      inputScalarRange[0] == outputScalarRange[0] &&
      inputScalarRange[1] == outputScalarRange[1])
    {
    // Voxels do not need to be recalculated
    if (input != output)
      {
      output->DeepCopy(input);
      }
    return;
    }

  vtkSmartPointer<vtkOrientedImageData> outputTemp;
  if (input == output)
    {
    outputTemp = vtkSmartPointer<vtkOrientedImageData>::New();
    }
  else
    {
    outputTemp = output;
    }

  vtkFractionalOperations::CopyFractionalParameters(outputTemp, outputTemplate);

  vtkSmartPointer<vtkMatrix4x4> imageToWorldMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  input->GetImageToWorldMatrix(imageToWorldMatrix);
  outputTemp->SetImageToWorldMatrix(imageToWorldMatrix);
  outputTemp->SetExtent(input->GetExtent());
  outputTemp->AllocateScalars(outputTemplate->GetScalarType(), 1);

  int extent[6] = {0, -1, 0, -1, 0, -1};
  output->GetExtent(extent);
  if (extent[1] > extent[0] ||
      extent[3] > extent[2] ||
      extent[5] > extent[4])
    {
    // Labelmap is empty
    return;
    }

  switch(input->GetScalarType())
    {
    vtkTemplateMacro(ConvertFractionalImageGeneric(input, outputTemp, (VTK_TT*)NULL));
    default:
      std::cerr << "ConvertFractionalImage: Unknown scalar type" << std::endl;
      return;
    }

}

template<class InputScalarType>
void vtkFractionalOperations::ConvertFractionalImageGeneric(vtkOrientedImageData* input, vtkOrientedImageData* output, InputScalarType* inputScalarTypePointer )
{
  vtkNotUsed(inputScalarTypePointer);

    switch(output->GetScalarType())
    {
    vtkTemplateMacro(ConvertFractionalImageGeneric2(input, output, inputScalarTypePointer, (VTK_TT*)NULL));
    default:
      std::cerr << "ConvertFractionalImageGeneric: Unknown scalar type" << std::endl;
      return;
    }
}

template<class InputScalarType, class OutputScalarType>
void vtkFractionalOperations::ConvertFractionalImageGeneric2(vtkOrientedImageData* input, vtkOrientedImageData* output, InputScalarType* inputScalarTypePointer, OutputScalarType* outputScalarTypePointer )
{
  vtkNotUsed(inputScalarTypePointer);
  vtkNotUsed(outputScalarTypePointer);

  InputScalarType* inputPtr = (InputScalarType*)input->GetScalarPointerForExtent(input->GetExtent());
  OutputScalarType* outputPtr = (OutputScalarType*)output->GetScalarPointerForExtent(output->GetExtent());

  double inputScalarRange[2] = {0.0,1.0};
  vtkFractionalOperations::GetScalarRange(input, inputScalarRange);

  double outputScalarRange[2] = {0.0,1.0};
  vtkFractionalOperations::GetScalarRange(output, outputScalarRange);

  int dimensions[3] = {0,0,0};
  input->GetDimensions(dimensions);

  int numberOfVoxels = dimensions[0] + dimensions[1] + dimensions[3];
  for (int i=0; i<numberOfVoxels; ++i)
    {
    double value = ((*inputPtr) - inputScalarRange[0]) / (inputScalarRange[1] - inputScalarRange[0]);
    (*outputPtr) = static_cast<OutputScalarType>(value * (outputScalarRange[1] - outputScalarRange[0]) + outputScalarRange[0]);
    ++inputPtr;
    ++outputPtr;
    }
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::CalculateOversampledGeometry(vtkOrientedImageData* input, vtkOrientedImageData* outputGeometry, int oversamplingFactor)
{
  double spacing[3] = {0,0,0};
  input->GetSpacing(spacing);

  int extent[6] = {0,-1,0,-1,0,-1};
  input->GetExtent(extent);

  outputGeometry->CopyDirections(input);

  vtkSmartPointer<vtkMatrix4x4> imageToWorldMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  input->GetImageToWorldMatrix(imageToWorldMatrix);

  double shift = -(oversamplingFactor-1.0)/(2.0*oversamplingFactor);
  double originIJK[4] = {shift, shift, shift, 1};
  double* originRAS = imageToWorldMatrix->MultiplyDoublePoint(originIJK);
  outputGeometry->SetOrigin(originRAS);

  outputGeometry->SetSpacing(spacing[0]/oversamplingFactor,
                             spacing[1]/oversamplingFactor,
                             spacing[2]/oversamplingFactor);

  outputGeometry->SetExtent(oversamplingFactor*extent[0], oversamplingFactor*extent[1]+oversamplingFactor-1,
                            oversamplingFactor*extent[2], oversamplingFactor*extent[3]+oversamplingFactor-1,
                            oversamplingFactor*extent[4], oversamplingFactor*extent[5]+oversamplingFactor-1);
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::ClearFractionalParameters(vtkOrientedImageData* input)
{
  if (!input)
  {
    std::cerr << "ClearFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetScalarRangeFieldName());
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetThresholdValueFieldName());
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetInterpolationTypeFieldName());
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::SetDefaultFractionalParameters(vtkOrientedImageData* input)
{
  if (!input)
  {
    std::cerr << "SetFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  vtkFractionalOperations::ClearFractionalParameters(input);

  vtkFractionalOperations::SetScalarRange(input, vtkFractionalOperations::DefaultScalarRange);
  vtkFractionalOperations::SetThreshold(input, vtkFractionalOperations::DefaultThreshold);
  vtkFractionalOperations::SetInterpolationType(input, vtkFractionalOperations::DefaultInterpolationType);
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::CopyFractionalParameters(vtkOrientedImageData* input, vtkOrientedImageData* originalLabelmap)
{
  if (!input || !originalLabelmap)
    {
    std::cerr << "CopyFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
    }

  vtkFractionalOperations::ClearFractionalParameters(input);

  vtkFieldData* inputFieldData = input->GetFieldData();
  vtkFieldData* originalFieldData = originalLabelmap->GetFieldData();

  if (inputFieldData && originalFieldData)
    {
    vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
    if (scalarRangeArray && scalarRangeArray->GetNumberOfComponents() == 2)
      {
      inputFieldData->AddArray(scalarRangeArray);
      }
    else
      {
      vtkFractionalOperations::SetScalarRange(input, vtkFractionalOperations::DefaultScalarRange);
      }

    vtkDoubleArray* thresholdArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetThresholdValueFieldName()));
    if (thresholdArray && thresholdArray->GetNumberOfComponents() == 1)
      {
      inputFieldData->AddArray(thresholdArray);
      }
    else
      {
      vtkFractionalOperations::SetThreshold(input, vtkFractionalOperations::DefaultThreshold);
      }

    vtkIntArray* interpolationTypeArray = vtkIntArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetInterpolationTypeFieldName()));
    if (interpolationTypeArray && interpolationTypeArray->GetNumberOfComponents() == 1)
      {
      inputFieldData->AddArray(interpolationTypeArray);
      }
    else
      {
      vtkFractionalOperations::SetInterpolationType(input, vtkFractionalOperations::DefaultInterpolationType);
      }
    }
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::CopyFractionalParameters(vtkOrientedImageData* input, vtkSegmentation* segmentation)
{
  if (!input)
    {
    std::cerr << "CopyFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
    }

  if (!segmentation)
    {
    std::cerr << "CopyFractionalParameters: invalid vtkSegmentation" << std::endl;
    return;
    }

  std::vector<std::string> segmentIds = std::vector<std::string>();
  segmentation->GetSegmentIDs(segmentIds);

  bool foundCompleteParamters = false;

  vtkFieldData* inputFieldData = input->GetFieldData();
  for (std::vector<std::string>::iterator segmentIDIt = segmentIds.begin(); segmentIDIt != segmentIds.end(); ++segmentIDIt)
    {
    vtkFractionalOperations::ClearFractionalParameters(input);

    vtkOrientedImageData* originalLabelmap = vtkOrientedImageData::SafeDownCast(segmentation->GetSegmentRepresentation(*segmentIDIt, vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));
    vtkFieldData* originalFieldData = originalLabelmap->GetFieldData();

    vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
    if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
      {
      inputFieldData->AddArray(scalarRangeArray);
      }
    else
      {
      continue;
      }

    vtkDoubleArray* thresholdArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetThresholdValueFieldName()));
    if (thresholdArray && thresholdArray->GetNumberOfValues() == 1)
      {
      inputFieldData->AddArray(thresholdArray);
      }
    else
      {
      continue;
      }

    vtkDoubleArray* interpolationTypeArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetInterpolationTypeFieldName()));
    if (interpolationTypeArray && interpolationTypeArray->GetNumberOfValues() == 1)
      {
      inputFieldData->AddArray(interpolationTypeArray);
      }
    else
      {
      continue;
      }

    foundCompleteParamters = true;
    }

  if (!foundCompleteParamters)
    {
    vtkFractionalOperations::SetDefaultFractionalParameters(input);
    }
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::GetScalarRange(vtkOrientedImageData* input, double scalarRange[2])
{
  if (!input)
    {
    std::cerr << "GetScalarRange: invalid vtkOrientedImageData" << std::endl;
    }

  if (!scalarRange)
    {
    std::cerr << "GetScalarRange: invalid double array" << std::endl;
    }

  vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
    {
    scalarRange[0] = scalarRangeArray->GetValue(0);
    scalarRange[1] = scalarRangeArray->GetValue(1);
    }
  else
    {
    scalarRange[0] = DefaultScalarRange[0];
    scalarRange[1] = DefaultScalarRange[1];
    }

}

//----------------------------------------------------------------------------
double vtkFractionalOperations::GetThreshold(vtkOrientedImageData* input)
{
  if (!input)
    {
    std::cerr << "GetThreshold: invalid vtkOrientedImageData" << std::endl;
    }

  double threshold = DefaultThreshold;

  vtkDoubleArray* thresholdArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetThresholdValueFieldName()));
  if (thresholdArray && thresholdArray->GetNumberOfValues() == 1)
    {
    threshold = thresholdArray->GetValue(0);
    }

  return threshold;
}

//----------------------------------------------------------------------------
vtkIdType vtkFractionalOperations::GetInterpolationType(vtkOrientedImageData* input)
{
  if (!input)
    {
    std::cerr << "GetInterpolationType: invalid vtkOrientedImageData" << std::endl;
    }

  vtkIdType interpolationType = DefaultInterpolationType;

  vtkDoubleArray* interpolationTypeArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetInterpolationTypeFieldName()));
  if (interpolationTypeArray && interpolationTypeArray->GetNumberOfValues() == 1)
    {
    interpolationType = interpolationTypeArray->GetValue(0);
    }

  return interpolationType;
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::GetScalarRange(vtkSegmentation* input, double scalarRange[2])
{
  if (!input)
    {
    std::cerr << "GetScalarRange: invalid vtkSegmentation" << std::endl;
    }

  if (!scalarRange)
    {
    std::cerr << "GetScalarRange: invalid double array" << std::endl;
    }

  vtkOrientedImageData* templateLablemap = vtkOrientedImageData::SafeDownCast(
    input->GetNthSegment(0)->GetRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));

  if (!templateLablemap)
    {
    scalarRange[0] = vtkFractionalOperations::DefaultScalarRange[0];
    scalarRange[1] = vtkFractionalOperations::DefaultScalarRange[1];
    return;
    }

  vtkFractionalOperations::GetScalarRange(templateLablemap, scalarRange);

}

//----------------------------------------------------------------------------
double vtkFractionalOperations::GetThreshold(vtkSegmentation* input)
{
  if (!input)
    {
    std::cerr << "GetThreshold: invalid vtkSegmentation" << std::endl;
    }

  vtkOrientedImageData* templateLablemap = vtkOrientedImageData::SafeDownCast(
    input->GetNthSegment(0)->GetRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));

  if (!templateLablemap)
    {
    return vtkFractionalOperations::DefaultThreshold;
    }

  return vtkFractionalOperations::GetThreshold(templateLablemap);
}

//----------------------------------------------------------------------------
vtkIdType vtkFractionalOperations::GetInterpolationType(vtkSegmentation* input)
{
  if (!input)
    {
    std::cerr << "GetInterpolationType: invalid vtkSegmentation" << std::endl;
    }

  vtkOrientedImageData* templateLablemap = vtkOrientedImageData::SafeDownCast(
    input->GetNthSegment(0)->GetRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));

  if (!templateLablemap)
    {
    return vtkFractionalOperations::DefaultInterpolationType;
    }

  return vtkFractionalOperations::GetInterpolationType(templateLablemap);
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::SetScalarRange(vtkOrientedImageData* input, const double scalarRange[2])
{
  if (!input)
    {
    std::cerr << "SetScalarRange: invalid vtkOrientedImageData" << std::endl;
    }

  if (!scalarRange)
    {
    std::cerr << "SetScalarRange: invalid double array" << std::endl;
    }

  // Remove the existing array
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetScalarRangeFieldName());

  // Specify the scalar range of values in the labelmap
  vtkSmartPointer<vtkDoubleArray> scalarRangeArray = vtkSmartPointer<vtkDoubleArray>::New();
  scalarRangeArray->SetName(vtkSegmentationConverter::GetScalarRangeFieldName());
  scalarRangeArray->InsertNextValue(scalarRange[0]);
  scalarRangeArray->InsertNextValue(scalarRange[1]);
  input->GetFieldData()->AddArray(scalarRangeArray);

}

//----------------------------------------------------------------------------
void vtkFractionalOperations::SetThreshold(vtkOrientedImageData* input, const double threshold)
{
  if (!input)
    {
    std::cerr << "SetThreshold: invalid vtkOrientedImageData" << std::endl;
    }

  // Remove the existing array
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetThresholdValueFieldName());

  // Specify the scalar range of values in the labelmap
  vtkSmartPointer<vtkDoubleArray> thresholdArray = vtkSmartPointer<vtkDoubleArray>::New();
  thresholdArray->SetName(vtkSegmentationConverter::GetThresholdValueFieldName());
  thresholdArray->InsertNextValue(threshold);
  input->GetFieldData()->AddArray(thresholdArray);

}

//----------------------------------------------------------------------------
void vtkFractionalOperations::SetInterpolationType(vtkOrientedImageData* input, const vtkIdType interpolationType)
{
  if (!input)
    {
    std::cerr << "SetInterpolationType: invalid vtkOrientedImageData" << std::endl;
    }

  // Remove the existing array
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetInterpolationTypeFieldName());

  // Specify the scalar range of values in the labelmap
  vtkSmartPointer<vtkIntArray> interpolationTypeArray = vtkSmartPointer<vtkIntArray>::New();
  interpolationTypeArray->SetName(vtkSegmentationConverter::GetInterpolationTypeFieldName());
  interpolationTypeArray->InsertNextValue(interpolationType);
  input->GetFieldData()->AddArray(interpolationTypeArray);
}

//----------------------------------------------------------------------------
vtkIdType vtkFractionalOperations::GetScalarType(vtkSegmentation* input)
{
  if (!input)
    {
    std::cerr << "GetScalarType: invalid vtkSegmentation" << std::endl;
    }

  vtkOrientedImageData* templateLablemap = vtkOrientedImageData::SafeDownCast(
    input->GetNthSegment(0)->GetRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));

  if (!templateLablemap || !vtkFractionalOperations::ContainsFractionalParameters(templateLablemap))
    {
    std::cerr << "GetScalarType: No fractional labelmaps in segmentation!" << std::endl;
    return vtkFractionalOperations::DefaultScalarType;
    }

    return templateLablemap->GetScalarType();

}

//----------------------------------------------------------------------------
bool vtkFractionalOperations::ContainsFractionalParameters(vtkOrientedImageData* input)
{
  if (!input)
  {
    return false;
  }

  vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (!scalarRangeArray || scalarRangeArray->GetNumberOfValues() != 2)
    {
    return false;
    }

  vtkDoubleArray* thresholdArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetThresholdValueFieldName()));
  if (!thresholdArray || thresholdArray->GetNumberOfValues() != 1)
    {
    return false;
    }

  vtkIntArray* interpolationTypeArray = vtkIntArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetInterpolationTypeFieldName()));
  if (!interpolationTypeArray || interpolationTypeArray->GetNumberOfValues() != 1)
    {
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
void vtkFractionalOperations::Write(vtkImageData* image, const char* name)
{
  vtkNew<vtkNRRDWriter> writer;
  writer->SetInputData(image);
  writer->SetFileName(name);
  writer->Update();
}