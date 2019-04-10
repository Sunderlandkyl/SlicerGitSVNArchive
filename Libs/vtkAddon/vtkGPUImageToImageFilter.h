/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUImageToImageFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGPUImageToImageFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkGPUImageToImageFilter_h
#define vtkGPUImageToImageFilter_h

#include "vtkAddon.h" // For export macro
#include "vtkObject.h"
#include "vtkAlgorithm.h"

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar

#include <vector>

class vtkOpenGLRenderWindow;
class vtkRenderWindow;
class vtkDataArray;
class vtkGPUImageData;
class vtkTextureObject;
class vtkImageData;
class vtkPixelBufferObject;

class VTK_ADDON_EXPORT vtkGPUImageToImageFilter : public vtkAlgorithm
{
public:
  static vtkGPUImageToImageFilter *New();
  vtkTypeMacro(vtkGPUImageToImageFilter, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
  * Get the output data object for a port on this algorithm.
  */
  vtkImageData* GetOutput();
  vtkImageData* GetOutput(int);
  void SetOutput(vtkImageData* d);
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
  vtkGPUImageData *GetInput(int index);
  //@}

  /**
   * Set the render window to get the OpenGL resources from
   */
  void SetRenderWindow(vtkRenderWindow *renWin);

  //@{
  /**
  * Set the desired output scalar type. The result of the shift
  * and scale operations is cast to the type specified.
  */
  vtkSetMacro(OutputScalarType, int);
  vtkGetMacro(OutputScalarType, int);
  void SetOutputScalarTypeToDouble()
  {
    this->SetOutputScalarType(VTK_DOUBLE);
  }
  void SetOutputScalarTypeToFloat()
  {
    this->SetOutputScalarType(VTK_FLOAT);
  }
  void SetOutputScalarTypeToLong()
  {
    this->SetOutputScalarType(VTK_LONG);
  }
  void SetOutputScalarTypeToUnsignedLong()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_LONG);
  };
  void SetOutputScalarTypeToInt()
  {
    this->SetOutputScalarType(VTK_INT);
  }
  void SetOutputScalarTypeToUnsignedInt()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_INT);
  }
  void SetOutputScalarTypeToShort()
  {
    this->SetOutputScalarType(VTK_SHORT);
  }
  void SetOutputScalarTypeToUnsignedShort()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_SHORT);
  }
  void SetOutputScalarTypeToSignedChar()
  {
    this->SetOutputScalarType(VTK_SIGNED_CHAR);
  }
  void SetOutputScalarTypeToUnsignedChar()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_CHAR);
  }
  //@}

 protected:
  vtkGPUImageToImageFilter();
  ~vtkGPUImageToImageFilter() VTK_OVERRIDE;

  int RequestInformation(vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  // Pipeline-related methods
  int RequestDataObject(vtkInformation*,
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

  //// These are called by the superclass.
  int RequestUpdateExtent(vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  int Execute(vtkGPUImageData* inputTexture, vtkImageData* outputImage);

  template<typename DATA_TYPE>
  int ExecuteInternal(vtkPixelBufferObject* inputPixelBuffer, vtkImageData* outputImage);

  // see vtkAlgorithm for docs.
  int FillInputPortInformation(int, vtkInformation*) override;
  int FillOutputPortInformation(int, vtkInformation*) override;

  vtkSmartPointer<vtkOpenGLRenderWindow> RenderWindow;
  vtkOpenGLHelper Quad;

  int OutputScalarType;

 private:
  vtkGPUImageToImageFilter(const vtkGPUImageToImageFilter&) = delete;
  void operator=(const vtkGPUImageToImageFilter&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkGPUImageToImageFilter.h
