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

#ifndef __vtkFractionalOperations_h
#define __vtkFractionalOperations_h

// Segmentation includes
#include "vtkSegmentationCoreConfigure.h"

#include "vtkObject.h"

class vtkOrientedImageData;
class vtkSegmentation;
class vtkStringArray;
class vtkImageData;

/// \ingroup SegmentationCore
/// \brief Utility functions for resampling oriented image data
class vtkSegmentationCore_EXPORT vtkFractionalOperations : public vtkObject
{

public:
  static vtkFractionalOperations *New();
  vtkTypeMacro(vtkFractionalOperations,vtkObject);

private:
  static const double DefaultScalarRange[2];
  static const double DefaultThreshold;
  static const vtkIdType DefaultInterpolationType;
  static const vtkIdType DefaultScalarType;

public:
  static void Invert(vtkOrientedImageData* labelmap);

  static void ConvertFractionalImage(vtkOrientedImageData* input, vtkOrientedImageData* output, vtkSegmentation* segmentationTemplate);
  static void ConvertFractionalImage(vtkOrientedImageData* input, vtkOrientedImageData* output, vtkOrientedImageData* outputTemplate);

  static void CalculateOversampledGeometry(vtkOrientedImageData* input, vtkOrientedImageData* outputGeometry, int oversamplingFactor);

  static void ClearFractionalParameters(vtkOrientedImageData* input);
  static void SetDefaultFractionalParameters(vtkOrientedImageData* input);
  static void CopyFractionalParameters(vtkOrientedImageData* input, vtkOrientedImageData* originalLabelmap);
  static void CopyFractionalParameters(vtkOrientedImageData* input, vtkSegmentation* segmentation);

  //TODO
  static void GetScalarRange(vtkSegmentation* input, double scalarRange[2]);
  static double GetThreshold(vtkSegmentation* input);
  static vtkIdType GetInterpolationType(vtkSegmentation* input);

  static void GetScalarRange(vtkOrientedImageData* input, double scalarRange[2]);
  static double GetThreshold(vtkOrientedImageData* input);
  static vtkIdType GetInterpolationType(vtkOrientedImageData* input);

  static void SetScalarRange(vtkOrientedImageData* input, const double scalarRange[2]);
  static void SetThreshold(vtkOrientedImageData* input, const double threshold);
  static void SetInterpolationType(vtkOrientedImageData* input, const vtkIdType interpolationType);

  static vtkIdType GetScalarType(vtkSegmentation* input);

  static bool ContainsFractionalParameters(vtkOrientedImageData* input);

  static void Write(vtkImageData* image, const char* name);

protected:
  template <class LabelmapScalarType>
  static void InvertGeneric(LabelmapScalarType* labelmapPointer, int dimensions[3], double scalarRange[2]);

  template <class InputScalarType>
  static void ConvertFractionalImageGeneric(vtkOrientedImageData* input, vtkOrientedImageData* output, InputScalarType*);
  template <class InputScalarType, class OutputScalarType>
  static void ConvertFractionalImageGeneric2(vtkOrientedImageData* input, vtkOrientedImageData* output, InputScalarType*, OutputScalarType*);

protected:
  vtkFractionalOperations();
  ~vtkFractionalOperations();

private:
  vtkFractionalOperations(const vtkFractionalOperations&);  // Not implemented.
  void operator=(const vtkFractionalOperations&);  // Not implemented.

};

#endif