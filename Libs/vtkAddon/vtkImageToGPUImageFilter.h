/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImageToGPUImageFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkImageToGPUImageFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkImageToGPUImageFilter_h
#define vtkImageToGPUImageFilter_h

#include "vtkAddon.h" // For export macro
#include "vtkObject.h"
#include "vtkAlgorithm.h"

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar
#include "vtkRenderWindow.h"

#include <vector>

class vtkOpenGLRenderWindow;
class vtkDataArray;
class vtkGPUImageData;
class vtkImageData;

class VTK_ADDON_EXPORT vtkImageToGPUImageFilter : public vtkAlgorithm
{
public:
  static vtkImageToGPUImageFilter *New();
  vtkTypeMacro(vtkImageToGPUImageFilter, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
  * Get the output data object for a port on this algorithm.
  */
  vtkGPUImageData* GetOutput();
  vtkGPUImageData* GetOutput(int);
  void SetOutput(vtkGPUImageData* d);
  //@}

  /**
  * Process a request from the executive.  For vtkImageAlgorithm, the
  * request will be delegated to one of the following methods: RequestData,
  * RequestInformation, or RequestUpdateExtent.
  */
  int ProcessRequest(vtkInformation*,
                     vtkInformationVector**,
                     vtkInformationVector*) VTK_OVERRIDE;

  //@{
  /**
  * Assign a data object as input. Note that this method does not
  * establish a pipeline connection. Use SetInputConnection to
  * setup a pipeline connection.
  */
  //void SetInputData(int index, vtkDataObject *);
  //void SetInputData(vtkDataObject *input) { this->SetInputData(0, input); };
  //@}

  //@{
  /**
  * Get a data object for one of the input port connections.  The use
  * of this method is strongly discouraged, but some filters that were
  * written a long time ago still use this method.
  */
  vtkImageData *GetInput(int index);
  //@}

  /**
   * Set the render window to get the OpenGL resources from
   */
  void SetRenderWindow(vtkRenderWindow *renWin);
  vtkRenderWindow* GetRenderWindow();

 protected:
  vtkImageToGPUImageFilter();
  ~vtkImageToGPUImageFilter() VTK_OVERRIDE;

  int RequestInformation(vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  // Pipeline-related methods
  int RequestDataObject(vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector*);

  /**
  * Subclasses can reimplement this method to translate the update
  * extent requests from each output port into update extent requests
  * for the input connections.
  */
  virtual int RequestUpdateExtent(vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector*);

  /**
   * This is called in response to a REQUEST_DATA request from the
   * executive.  Subclasses should override either this method or the
   * ExecuteDataWithInformation method in order to generate data for
   * their outputs.  For images, the output arrays will already be
   * allocated, so all that is necessary is to fill in the voxel values.
   */
  virtual int RequestData(vtkInformation *request,
                          vtkInformationVector** inputVector,
                          vtkInformationVector* outputVector);

  // see vtkAlgorithm for docs.
  int FillInputPortInformation(int, vtkInformation*) override;
  int FillOutputPortInformation(int, vtkInformation*) override;

  vtkSmartPointer<vtkOpenGLRenderWindow> RenderWindow;
  vtkOpenGLHelper Quad;

private:
  vtkImageToGPUImageFilter(const vtkImageToGPUImageFilter&) = delete;
  void operator=(const vtkImageToGPUImageFilter&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkImageToGPUImageFilter.h
