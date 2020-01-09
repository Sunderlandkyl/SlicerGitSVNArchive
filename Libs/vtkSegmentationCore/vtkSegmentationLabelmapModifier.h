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

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

#ifndef __vtkSegmentationLabelmapModifier_h
#define __vtkSegmentationLabelmapModifier_h

// Segmentation includes
#include "vtkSegmentationCoreConfigure.h"

// VTK includes
#include "vtkObject.h"

// std includes
#include <vector>

class vtkSegmentation;
class vtkOrientedImageData;

/// \ingroup SegmentationCore
/// \brief Utility functions for resampling oriented image data
class vtkSegmentationCore_EXPORT vtkSegmentationLabelmapModifier : public vtkObject
{
public:
  static vtkSegmentationLabelmapModifier *New();
  vtkTypeMacro(vtkSegmentationLabelmapModifier,vtkObject);

  /// TODO
  static bool ModifySegmentationByLabelmap(vtkSegmentation* segmentation, std::vector<std::string>& segmentIds,
    vtkOrientedImageData* labelmap, vtkOrientedImageData* mask);

protected:
  vtkSegmentationLabelmapModifier();
  ~vtkSegmentationLabelmapModifier() override;

private:
  vtkSegmentationLabelmapModifier(const vtkSegmentationLabelmapModifier&) = delete;
  void operator=(const vtkSegmentationLabelmapModifier&) = delete;
};

#endif
