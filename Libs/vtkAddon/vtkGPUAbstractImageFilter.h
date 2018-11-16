/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUAbstractImageFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGPUAbstractImageFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkGPUAbstractImageFilter_h
#define vtkGPUAbstractImageFilter_h

#include "vtkAddon.h" // For export macro
#include "vtkAlgorithm.h"
#include "vtkObject.h"
#include "vtkOpenGLHelper.h"
#include "vtkShader.h"
#include "vtkSmartPointer.h"

#include <map>
#include <vector>

class vtkGPUImageData;
class vtkOpenGLRenderWindow;
class vtkOpenGLShaderProperty;
class vtkRenderWindow;

class VTK_ADDON_EXPORT vtkGPUAbstractImageFilter : public vtkAlgorithm
{
public:
  static vtkGPUAbstractImageFilter *New();
  vtkTypeMacro(vtkGPUAbstractImageFilter, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
  * Get the output data object for a port on this algorithm.
  */
  vtkGPUImageData* GetOutput();
  vtkGPUImageData* GetOutput(int);
  void SetOutput(vtkDataObject* d);
  //@}

  /**
  * Process a request from the executive.  For vtkImageAlgorithm, the
  * request will be delegated to one of the following methods: RequestData,
  * RequestInformation, or RequestUpdateExtent.
  */
  int ProcessRequest(vtkInformation*,
                     vtkInformationVector**,
                     vtkInformationVector*) VTK_OVERRIDE;

  /**
  * Subclasses can reimplement this method to translate the update
  * extent requests from each output port into update extent requests
  * for the input connections.
  */
  virtual int RequestUpdateExtent(vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector*);

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

  //@{
  /**
  * Assign a data object as input. Note that this method does not
  * establish a pipeline connection. Use SetInputConnection to
  * setup a pipeline connection.
  */
  void AddInputData(vtkGPUImageData *);

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

  vtkGetMacro(NumberOfComponents, int);
  vtkSetMacro(NumberOfComponents, int);

 protected:
  vtkGPUAbstractImageFilter();
  ~vtkGPUAbstractImageFilter() VTK_OVERRIDE;

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

  bool ShaderRebuildNeeded();
  void BuildShader(std::vector<vtkGPUImageData*> inputTextures, vtkGPUImageData* outputTexture);

  int Execute(std::vector<vtkGPUImageData*> inputTextures, vtkGPUImageData* outputTexture);

  void ReplaceShaderCustomUniforms(std::map<vtkShader::Type, vtkShader*>& shaders, vtkOpenGLShaderProperty * p);
  void ReplaceShaderTextureInput(std::map<vtkShader::Type, vtkShader*>& shaders, std::vector<vtkGPUImageData*> inputTextures, vtkGPUImageData* outputTexture);

  void UpdateTextureUniforms(std::vector<vtkGPUImageData*> inputTextures, vtkGPUImageData* outputTexture);

  // see vtkAlgorithm for docs.
  int FillInputPortInformation(int, vtkInformation*) override;
  int FillOutputPortInformation(int, vtkInformation*) override;

  void ShaderPropertyModifed(vtkObject*, unsigned long, void*);

  vtkSmartPointer<vtkOpenGLRenderWindow> RenderWindow;
  vtkNew<vtkOpenGLShaderProperty> ShaderProperty;

  vtkOpenGLHelper Quad;

  int OutputScalarType;

  int OutputExtent[6];
  bool OutputExtentSpecified;

  int NumberOfComponents;

  std::string DefaultVertexShaderSource;
  std::string DefaultFragmentShaderSource;
  std::string DefaultGeometryShaderSource;

  vtkShaderProgram* ShaderProgram;
  vtkTimeStamp ShaderBuildTime;

 private:
  vtkGPUAbstractImageFilter(const vtkGPUAbstractImageFilter&) = delete;
  void operator=(const vtkGPUAbstractImageFilter&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkGPUAbstractImageFilter.h
