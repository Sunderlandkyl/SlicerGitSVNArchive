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

#ifndef __vtkResampleBinaryLabelmapToFractionalLabelmap_h
#define __vtkResampleBinaryLabelmapToFractionalLabelmap_h

// VTK includes
#include <vtkImageAlgorithm.h>

// Segmentation includes
#include "vtkOrientedImageData.h"
#include "vtkSegmentationCoreConfigure.h"

/// \ingroup SegmentationCore
/// \brief Utility functions for resampling oriented image data
class vtkSegmentationCore_EXPORT vtkResampleBinaryLabelmapToFractionalLabelmap : public vtkImageAlgorithm
{

public:
  static vtkResampleBinaryLabelmapToFractionalLabelmap *New();
  vtkTypeMacro(vtkResampleBinaryLabelmapToFractionalLabelmap,vtkAlgorithm);

  vtkSetMacro(OversamplingFactor, int)
  vtkGetMacro(OversamplingFactor, int)

  vtkSetMacro(OutputScalarType, vtkIdType);
  vtkGetMacro(OutputScalarType, vtkIdType);

  vtkSetMacro(OutputMinimumValue, double);
  vtkGetMacro(OutputMinimumValue, double);

  vtkSetMacro(StepSize, double);
  vtkGetMacro(StepSize, double);

  virtual vtkOrientedImageData* GetOutput();
  virtual void SetOutput(vtkOrientedImageData* output);

protected:
  int RequestData(vtkInformation *, vtkInformationVector **, vtkInformationVector *);
  virtual int FillOutputPortInformation(int, vtkInformation*);
  virtual int FillInputPortInformation(int, vtkInformation*);

private:
  int OversamplingFactor;
  vtkIdType OutputScalarType;
  double OutputMinimumValue;
  double StepSize;

protected:
  vtkResampleBinaryLabelmapToFractionalLabelmap();
  ~vtkResampleBinaryLabelmapToFractionalLabelmap();

private:
  vtkResampleBinaryLabelmapToFractionalLabelmap(const vtkResampleBinaryLabelmapToFractionalLabelmap&);  // Not implemented.
  void operator=(const vtkResampleBinaryLabelmapToFractionalLabelmap&);  // Not implemented.

};

#endif