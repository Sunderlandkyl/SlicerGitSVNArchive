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

#ifndef __vtkFractionalLogicalOperations_h
#define __vtkFractionalLogicalOperations_h

// Segmentation includes
#include "vtkSegmentationCoreConfigure.h"

#include "vtkObject.h"

class vtkOrientedImageData;
class vtkSegmentation;
class vtkStringArray;
class vtkImageData;

/// \ingroup SegmentationCore
/// \brief Utility functions for resampling oriented image data
class vtkSegmentationCore_EXPORT vtkFractionalLogicalOperations : public vtkObject
{

public:
  static vtkFractionalLogicalOperations *New();
  vtkTypeMacro(vtkFractionalLogicalOperations,vtkObject);

public:
  static void Invert(vtkOrientedImageData* labelmap);

  static void Union(vtkOrientedImageData* output, vtkOrientedImageData* a, vtkOrientedImageData* b);
  static void Union(vtkOrientedImageData* output, vtkSegmentation* segmentation, vtkStringArray* segmentIds);

  static void ClearFractionalParameters(vtkOrientedImageData* input);
  static void SetDefaultFractionalParameters(vtkOrientedImageData* input);
  static void CopyFractionalParameters(vtkOrientedImageData* input, vtkOrientedImageData* originalLabelmap);
  static void CopyFractionalParameters(vtkOrientedImageData* input, vtkSegmentation* segmentation);

  //static void GetScalarRange(vtkOrientedImageData* input, double scalarRange[2]);


  static void Write(vtkImageData* image, const char* name);

protected:
  template <class T>
  static void InvertGeneric(T* labelmapPointer, int dimensions[3], double scalarRange[2]);

protected:
  vtkFractionalLogicalOperations();
  ~vtkFractionalLogicalOperations();

private:
  vtkFractionalLogicalOperations(const vtkFractionalLogicalOperations&);  // Not implemented.
  void operator=(const vtkFractionalLogicalOperations&);  // Not implemented.

};

#endif