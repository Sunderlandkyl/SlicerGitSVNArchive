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
  and was supported through CANARIE's Research Software Program, and Cancer
  Care Ontario.

==============================================================================*/

// SlicerQt includes
#include "qSlicerApplication.h"
#include "qSlicerCoreIOManager.h"
#include <qSlicerNodeWriter.h>

// Texts includes
#include "qSlicerTextsModule.h"
#include "qSlicerTextsModuleWidget.h"
#include "qSlicerTextsReader.h"
#include "vtkSlicerTextsLogic.h"

// VTK includes
#include "vtkSmartPointer.h"

//-----------------------------------------------------------------------------
class qSlicerTextsModulePrivate
{
public:
};

//-----------------------------------------------------------------------------
qSlicerTextsModule::qSlicerTextsModule(QObject* _parentObject)
  : Superclass(_parentObject)
  , d_ptr(new qSlicerTextsModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerTextsModule::~qSlicerTextsModule() = default;

//-----------------------------------------------------------------------------
QIcon qSlicerTextsModule::icon()const
{
  return QIcon(":/Icons/Texts.png");
}


//-----------------------------------------------------------------------------
QStringList qSlicerTextsModule::categories() const
{
  return QStringList()
    << "";
}

//-----------------------------------------------------------------------------
QStringList qSlicerTextsModule::dependencies() const
{
  return QStringList();
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerTextsModule::createWidgetRepresentation()
{
  return new qSlicerTextsModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerTextsModule::createLogic()
{
  return vtkSlicerTextsLogic::New();
}

//-----------------------------------------------------------------------------
QString qSlicerTextsModule::helpText()const
{
  QString help =
    "The Texts Module creates and edits Texts.";
  return help;
}

//-----------------------------------------------------------------------------
QString qSlicerTextsModule::acknowledgementText()const
{
  QString acknowledgement =
    "This work was supported by CANARIE, and the Slicer Community.<br>"
    "See <a href=\"http://www.slicer.org\">www.slicer.org</a> for details.<br>"
    "The Texts module was contributed by Kyle Sunderland and Andras Lasso, Perk Lab, Queen's University ";
  return acknowledgement;
}

//-----------------------------------------------------------------------------
QStringList qSlicerTextsModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Kyle Sunderland (PerkLab, Queen's)");
  moduleContributors << QString("Andras Lasso (PerkLab, Queen's)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
void qSlicerTextsModule::setup()
{
  qSlicerApplication * app = qSlicerApplication::application();
  if (!app)
    {
    return;
    }

  vtkSlicerTextsLogic* textsLogic =
    vtkSlicerTextsLogic::SafeDownCast(this->logic());
  qSlicerTextsReader* textFileReader = new qSlicerTextsReader(this);
  app->coreIOManager()->registerIO(textFileReader);
  app->coreIOManager()->registerIO(new qSlicerNodeWriter("TextFileImporter", textFileReader->fileType(), QStringList() << "vtkMRMLTextNode", false, this));
}

//-----------------------------------------------------------------------------
QStringList qSlicerTextsModule::associatedNodeTypes() const
{
  return QStringList()
    << "vtkMRMLTextNode";
}
