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

  /// Invert the values in the fractional labelmap according to the formula:
  ///   invertedValue = ScalarMax - value + ScalarMin;
  /// \param labelmap that is being inverted and has scalar range fractional parameter
  static void Invert(vtkOrientedImageData* labelmap);

  /// Calculate the geometry of a labelmap oversampled at the specified oversampling factor.
  /// \param input Labelmap to be oversampled
  /// \param outputGeometry Labelmap containing oversampled geometry
  /// \param The specified oversampling factor
  static void CalculateOversampledGeometry(vtkOrientedImageData* input, vtkOrientedImageData* outputGeometry, int oversamplingFactor);

public:
  /* Fractional labelmap parameter methods */

  /// Convert the input fraction image into the same scalar range and type as the template.
  /// Parameters are retrieved from the 0th segment (if it exists), otherwise uses default values.
  /// \param input The labelmap to be converted
  /// \param output The converted labelmap
  /// \param segmentationTemplate The segmentation containing the desired parameters
  static void ConvertFractionalImage(vtkOrientedImageData* input, vtkOrientedImageData* output, vtkSegmentation* segmentationTemplate);

  /// Convert the input fraction image into the same scalar range and type as the template.
  /// \param input The labelmap to be converted
  /// \param output The converted labelmap
  /// \param segmentationTemplate The labelmap containing the desired parameters
  static void ConvertFractionalImage(vtkOrientedImageData* input, vtkOrientedImageData* output, vtkOrientedImageData* outputTemplate);

  /// Determine if the specified vtkOrientedImageData contains fractional parameters
  /// \return True if all three parameters (scalar range, threshold value, interpolation type) are set, false otherwise
  static bool ContainsFractionalParameters(vtkOrientedImageData* input);

  /// Removes the fractional parameters from the specified image.
  /// \param input The labelmap to remove fractional parameters from
  static void ClearFractionalParameters(vtkOrientedImageData* input);

  /// Sets the fractional parameters in the specified image to default values.
  /// \param input The labelmap that the fractional parameters are applied to.
  static void SetDefaultFractionalParameters(vtkOrientedImageData* input);

  /// Copies fractional parameters to the input labelmap from the template.
  /// \param input Labelmap that the parameters are applied to.
  /// \param originalLabelmap Labelmap containing the specified parameters (if they exist, otherwise use default)
  static void CopyFractionalParameters(vtkOrientedImageData* input, vtkOrientedImageData* originalLabelmap);

  /// Copies fractional parameters to the input labelmap from the template.
  /// The parameters are copied from the 0th labelmap.
  /// \param input Labelmap that the parameters are applied to
  /// \param originalLabelmap Segmentation containing the specified parameters (if they exist, otherwise use default)
  static void CopyFractionalParameters(vtkOrientedImageData* input, vtkSegmentation* segmentation);

public:
  /* Fractional labelmap parameter get/set methods */

  /// Get the scalar range parameter from the specified segmentation.
  /// The parameter is retreived from the 0th segment (if it exists), otherwise, it returns the default parameters.
  /// \param input The segmentation containing a labelmap with the desired scalar range
  /// \param scalarRange The output scalar range array
  static void GetScalarRange(vtkSegmentation* input, double scalarRange[2]);

  /// Get the threshold value parameter from the specified segmentation.
  /// The parameter is retreived from the 0th segment (if it exists), otherwise, it returns the default parameters.
  /// \param input The segmentation containing a labelmap with the desired threshold value
  /// \return The segmentation threshold value
  static double GetThreshold(vtkSegmentation* input);

  /// Get the interpolation type parameter from the specified segmentation.
  /// The parameter is retreived from the 0th segment (if it exists), otherwise, it returns the default parameters.
  /// \param input The segmentation containing a labelmap with the desired interpolation type
  /// \return The segmentation interpolation type
  static vtkIdType GetInterpolationType(vtkSegmentation* input);

  /// Get the scalar range parameter from the specified vtkOrientedImageData.
  /// The parameter is retreived from the labelmap (if it exists), otherwise, it returns the default parameters.
  /// \param input The labelmap with the desired scalar range
  /// \param scalarRange The output scalar range array
  static void GetScalarRange(vtkOrientedImageData* input, double scalarRange[2]);

  /// Get the threshold value parameter from the specified vtkOrientedImageData.
  /// The parameter is retreived from the labelmap (if it exists), otherwise, it returns the default parameters.
  /// \param input The labelmap with the desired threshold value
  /// \return The segmentation threshold value
  static double GetThreshold(vtkOrientedImageData* input);

  /// Get the interpolation type parameter from the specified vtkOrientedImageData.
  /// The parameter is retreived from the labelmap (if it exists), otherwise, it returns the default parameters.
  /// \param input The labelmap with the desired interpolation type
  /// \param The segmentation interpolation type
  static vtkIdType GetInterpolationType(vtkOrientedImageData* input);

  /// Set the scalar range parameter from the specified vtkOrientedImageData.
  /// \param input The labelmap that the scalar range parameter added to
  /// \param scalarRange The input scalar range array
  static void SetScalarRange(vtkOrientedImageData* input, const double scalarRange[2]);

  /// Set the threshold value parameter from the specified vtkOrientedImageData.
  /// \param input The labelmap that the threshold value parameter added to
  /// \param threshold The input threshold value
  static void SetThreshold(vtkOrientedImageData* input, const double threshold);

  /// Set the interpolation type parameter from the specified vtkOrientedImageData.
  /// \param input The labelmap that the interpolation type parameter added to
  /// \param interpolationType The input interpolation type
  static void SetInterpolationType(vtkOrientedImageData* input, const vtkIdType interpolationType);

  /// Get the scalar type from the sepcified segmentation.
  /// The scalar type is retreived from the 0th segment.
  /// \param input The segmentation that the scalar type is retreived from
  /// \return The scalar type of the segmentation
  static vtkIdType GetScalarType(vtkSegmentation* input);

  static double GetScalarComponentAsFraction(vtkOrientedImageData* labelmap, int x, int y, int z, int component);

  /// TODO
  static double GetValueAsFraction(vtkOrientedImageData* labelmap, double value);
  static double GetValueAsFraction(double scalarRange[2], double value);

  /// TODO
  static void VoxelContentsConstraintMask(vtkOrientedImageData* modifierLabelmap, vtkOrientedImageData* mergedLabelmap,
    vtkOrientedImageData* segmentLabelmap, vtkOrientedImageData* outputLabelmap, int effectiveExtent[6]=NULL);

  /// TODO: remove, only used for testing
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