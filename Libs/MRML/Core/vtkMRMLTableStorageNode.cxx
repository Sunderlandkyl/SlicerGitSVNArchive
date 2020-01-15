/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright 2015 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Andras Lasso (PerkLab, Queen's
  University) and Kevin Wang (Princess Margaret Hospital, Toronto) and was
  supported through OCAIRO and the Applied Cancer Research Unit program of
  Cancer Care Ontario.

==============================================================================*/

// MRML includes
#include "vtkMRMLTableStorageNode.h"
#include "vtkMRMLTableNode.h"
#include "vtkMRMLScene.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkDelimitedTextReader.h>
#include <vtkDelimitedTextWriter.h>
#include <vtkErrorSink.h>
#include <vtkTable.h>
#include <vtkStringArray.h>
#include <vtkBitArray.h>
#include <vtkNew.h>
#include <vtksys/SystemTools.hxx>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLTableStorageNode);

//----------------------------------------------------------------------------
vtkMRMLTableStorageNode::vtkMRMLTableStorageNode()
{
  this->DefaultWriteFileExtension = "tsv";
  this->AutoFindSchema = true;
}

//----------------------------------------------------------------------------
vtkMRMLTableStorageNode::~vtkMRMLTableStorageNode()
= default;

//----------------------------------------------------------------------------
void vtkMRMLTableStorageNode::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkMRMLStorageNode::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
bool vtkMRMLTableStorageNode::CanReadInReferenceNode(vtkMRMLNode *refNode)
{
  return refNode->IsA("vtkMRMLTableNode");
}

//----------------------------------------------------------------------------
int vtkMRMLTableStorageNode::ReadDataInternal(vtkMRMLNode *refNode)
{
  std::string fullName = this->GetFullNameFromFileName();

  if (fullName.empty())
    {
    vtkErrorMacro("ReadData: File name not specified");
    return 0;
    }
  vtkMRMLTableNode *tableNode = vtkMRMLTableNode::SafeDownCast(refNode);
  if (tableNode == nullptr)
    {
    vtkErrorMacro("ReadData: unable to cast input node " << refNode->GetID() << " to a table node");
    return 0;
    }

  // Check that the file exists
  if (vtksys::SystemTools::FileExists(fullName) == false)
    {
    vtkErrorMacro("ReadData: table file '" << fullName << "' not found.");
    return 0;
    }

  if (this->GetSchemaFileName().empty() && this->AutoFindSchema)
    {
    this->SetSchemaFileName(this->FindSchemaFileName(fullName.c_str()).c_str());
    }
  if (!this->GetSchemaFileName().empty())
    {
    if (!this->ReadSchema(this->GetSchemaFileName(), tableNode))
      {
      vtkErrorMacro("ReadData: failed to read table schema from '" << this->GetSchemaFileName() << "'");
      return 0;
      }
    }

  if (!this->ReadTable(fullName, tableNode))
    {
    vtkErrorMacro("ReadData: failed to read table from '" << fullName << "'");
    return 0;
    }

  vtkDebugMacro("ReadData: successfully read table from file: " << fullName);
  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLTableStorageNode::WriteDataInternal(vtkMRMLNode *refNode)
{
  if (this->GetFileName() == nullptr)
    {
    vtkErrorMacro("WriteData: file name is not set");
    return 0;
    }
  std::string fullName = this->GetFullNameFromFileName();
  if (fullName.empty())
    {
      vtkErrorMacro("WriteData: file name not specified");
    return 0;
    }

  vtkMRMLTableNode *tableNode = vtkMRMLTableNode::SafeDownCast(refNode);
  if (tableNode == nullptr)
    {
    vtkErrorMacro("WriteData: unable to cast input node " << refNode->GetID() << " to a valid table node");
    return 0;
    }

  if (!this->WriteTable(fullName, tableNode))
    {
    vtkErrorMacro("WriteData: failed to write table node " << refNode->GetID() << " to file " << fullName);
    return 0;
    }
  vtkDebugMacro("WriteData: successfully wrote table to file: " << fullName);

  // Only write a schema file if some table properties are specified
  bool needToWriteSchema = (!this->GetSchemaFileName().empty()) || (tableNode->GetSchema() != nullptr);
  if (!needToWriteSchema && tableNode->GetTable() != nullptr)
    {
    // Make sure we create a schema file if there is any non-string column type
    vtkTable* table = tableNode->GetTable();
    for (int col = 0; col < table->GetNumberOfColumns(); ++col)
      {
      vtkAbstractArray* column = table->GetColumn(col);
      if (column == nullptr)
        {
        // invalid column
        continue;
        }
      if (column->GetDataType() != VTK_STRING)
        {
        needToWriteSchema = true;
        break;
        }
      }
    }

  if (needToWriteSchema)
    {
    std::string schemaFileName = this->GenerateSchemaFileName(fullName.c_str());
    this->SetSchemaFileName(schemaFileName.c_str());
    if (!this->WriteSchema(schemaFileName, tableNode))
      {
      vtkErrorMacro("WriteData: failed to write table node " << refNode->GetID() << " schema  to file " << schemaFileName);
      return 0;
      }
    vtkDebugMacro("WriteData: successfully wrote schema to file: " << schemaFileName);
    }

  return 1;
}

//----------------------------------------------------------------------------
void vtkMRMLTableStorageNode::InitializeSupportedReadFileTypes()
{
  this->SupportedReadFileTypes->InsertNextValue("Tab-separated values (.tsv)");
  this->SupportedReadFileTypes->InsertNextValue("Comma-separated values (.csv)");
  this->SupportedReadFileTypes->InsertNextValue("Text (.txt)");
}

//----------------------------------------------------------------------------
void vtkMRMLTableStorageNode::InitializeSupportedWriteFileTypes()
{
  this->SupportedWriteFileTypes->InsertNextValue("Tab-separated values (.tsv)");
  this->SupportedWriteFileTypes->InsertNextValue("Comma-separated values (.csv)");
  this->SupportedWriteFileTypes->InsertNextValue("Text (.txt)");
}

//----------------------------------------------------------------------------
void vtkMRMLTableStorageNode::SetSchemaFileName(const char* schemaFileName)
{
  this->ResetFileNameList();
  this->AddFileName(schemaFileName);
}

//----------------------------------------------------------------------------
std::string vtkMRMLTableStorageNode::GetSchemaFileName()
{
  const char* schemaFileNamePtr = this->GetNthFileName(0);
  return (schemaFileNamePtr ? schemaFileNamePtr : "");
}

//----------------------------------------------------------------------------
std::string vtkMRMLTableStorageNode::FindSchemaFileName(const char* filePath)
{
  std::string expectedSchemaFileName = this->GenerateSchemaFileName(filePath);
  if (!vtksys::SystemTools::FileExists(expectedSchemaFileName))
    {
    // schema file not found
    return "";
    }
  return expectedSchemaFileName;
}

//----------------------------------------------------------------------------
std::string vtkMRMLTableStorageNode::GenerateSchemaFileName(const char* filePath)
{
  std::string filePathStd = (filePath ? filePath : "");
  if (filePathStd.empty())
    {
    // If filePath is not specified then use current filename
    filePathStd = (this->GetFileName() ? this->GetFileName() : "");
    }
  if (filePathStd.empty())
    {
    return "";
    }
  std::string fileName = vtksys::SystemTools::GetFilenameName(filePathStd);
  std::string extension = this->GetSupportedFileExtension(fileName.c_str());

  if (fileName.length() < extension.length() ||
    fileName.compare(fileName.length() - extension.length(), extension.length(), extension) != 0)
    {
    // extension not matched to the end of filename
    return "";
    }

  // Insert .schema before file extension (something.csv => something.schema.csv)
  filePathStd.insert(filePathStd.length() - extension.length(), + ".schema");

  return filePathStd;
}

//----------------------------------------------------------------------------
std::string vtkMRMLTableStorageNode::GetFieldDelimiterCharacters(std::string filename)
{
  std::string lowercaseFileExt = vtkMRMLStorageNode::GetLowercaseExtensionFromFileName(filename);
  std::string fieldDelimiterCharacters;
  if (lowercaseFileExt == std::string(".tsv") || lowercaseFileExt == std::string(".txt"))
    {
    fieldDelimiterCharacters = "\t";
    }
  else if (lowercaseFileExt == std::string(".csv"))
    {
    fieldDelimiterCharacters = ",";
    }
  else
    {
    vtkErrorMacro("Cannot determine field delimiter character from file extension: " << lowercaseFileExt);
    }
  return fieldDelimiterCharacters;
}

//----------------------------------------------------------------------------
bool vtkMRMLTableStorageNode::ReadSchema(std::string filename, vtkMRMLTableNode* tableNode)
{
  if (filename.empty())
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::ReadSchema failed: filename not specified");
    return false;
    }

  if (vtksys::SystemTools::FileExists(filename) == false)
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::ReadSchema failed: schema file '" << filename << "' not found.");
    return false;
    }

  vtkNew<vtkDelimitedTextReader> reader;
  reader->SetFileName(filename.c_str());
  reader->SetHaveHeaders(true);
  reader->SetFieldDelimiterCharacters(this->GetFieldDelimiterCharacters(filename).c_str());
  // Make sure string delimiter characters are removed (somebody may have written a tsv with string delimiters)
  reader->SetUseStringDelimiter(true);
  // File contents is preserved better if we don't try to detect numeric columns
  reader->DetectNumericColumnsOff();

  // Read table
  vtkTable* schemaTable = nullptr;
  try
    {
    reader->Update();
    schemaTable = reader->GetOutput();
    }
  catch (...)
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::ReadSchema failed from file: " << filename);
    return false;
    }

  vtkStringArray* columnNameArray = vtkStringArray::SafeDownCast(schemaTable->GetColumnByName("columnName"));
  if (columnNameArray == nullptr)
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::ReadSchema failed from file: " << filename <<". Required 'columnName' column is not found in schema.");
    return false;
    }

  tableNode->SetAndObserveSchema(schemaTable);

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLTableStorageNode::ReadTable(std::string filename, vtkMRMLTableNode* tableNode)
{
  vtkNew<vtkDelimitedTextReader> reader;
  reader->SetFileName(filename.c_str());
  reader->SetHaveHeaders(true);
  reader->SetFieldDelimiterCharacters(this->GetFieldDelimiterCharacters(filename).c_str());
  // Make sure string delimiter characters are removed (somebody may have written a tsv with string delimiters)
  reader->SetUseStringDelimiter(true);
  // File contents is preserved better if we don't try to detect numeric columns
  reader->DetectNumericColumnsOff();

  // Read table
  vtkTable* rawTable = nullptr;
  try
    {
    reader->Update();
    rawTable = reader->GetOutput();
    }
  catch (...)
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::ReadTable: failed to read table file: " << filename);
    return 0;
    }

  // Struct for managing columns
  typedef struct
  {
    std::string ColumnName = "";
    std::vector<vtkAbstractArray*> RawComponentArrays;
    int DataType = -1;
    std::vector<std::string> ComponentNames;
  } ColumnInfo;

  vtkTable* schema = tableNode->GetSchema();
  std::vector<ColumnInfo> columnDetails;

  vtkStringArray* schemaColumnNameArray = nullptr;
  vtkStringArray* schemaComponentNamesArray = nullptr;
  if (schema != nullptr)
    {
    schemaColumnNameArray = vtkStringArray::SafeDownCast(schema->GetColumnByName("columnName"));
    schemaComponentNamesArray = vtkStringArray::SafeDownCast(schema->GetColumnByName("componentNames"));
    }

  // Populate the output table column details.
  // If the schema exists, read the contents and determine column data type/component names/component arrays
  if (schema != nullptr &&
      schemaColumnNameArray != nullptr &&
      schemaComponentNamesArray != nullptr)
    {
    for (int schemaRowIndex = 0; schemaRowIndex < schema->GetNumberOfRows(); ++schemaRowIndex)
      {
      ColumnInfo columnInfo;
      columnInfo.ColumnName = schemaColumnNameArray->GetValue(schemaRowIndex);
      columnInfo.DataType = tableNode->GetColumnValueTypeFromSchema(columnInfo.ColumnName);

      std::vector<vtkAbstractArray*> componentArrays;
      std::string componentNamesStr = schemaComponentNamesArray->GetValue(schemaRowIndex);
      if (componentNamesStr.empty())
        {
        vtkAbstractArray* rawColumn = rawTable->GetColumnByName(columnInfo.ColumnName.c_str());
        componentArrays.push_back(rawColumn);
        }
      else
        {
        std::stringstream ss(componentNamesStr);
        std::string componentName;
        while (std::getline(ss, componentName, '|'))
          {
          std::string componentColumnName = columnInfo.ColumnName + "_" + componentName;
          vtkAbstractArray* rawColumn = rawTable->GetColumnByName(componentColumnName.c_str());
          componentArrays.push_back(rawColumn);
          columnInfo.ComponentNames.push_back(componentName);
          }
        }
      columnInfo.RawComponentArrays = componentArrays;
      columnDetails.push_back(columnInfo);
      }
    }
  else
    {
    for (int col = 0; col < rawTable->GetNumberOfColumns(); ++col)
      {
      ColumnInfo columnInfo;
      vtkStringArray* column = vtkStringArray::SafeDownCast(rawTable->GetColumn(col));
      if (column == nullptr)
        {
        // invalid column
        continue;
        }
      if (!column->GetName())
        {
        vtkWarningMacro("vtkMRMLTableStorageNode::ReadTable: empty column name in file: " << filename << ", skipping column");
        continue;
        }
      columnInfo.ColumnName = column->GetName();
      columnInfo.DataType = tableNode->GetColumnValueTypeFromSchema(columnInfo.ColumnName);
      columnInfo.RawComponentArrays.push_back(column);
      columnDetails.push_back(columnInfo);
      }
    }

  vtkSmartPointer<vtkTable> table = vtkSmartPointer<vtkTable>::New();
  for (ColumnInfo columnInfo : columnDetails)
    {
    std::string columnName = columnInfo.ColumnName;
    int valueTypeId = columnInfo.DataType;
    std::vector<vtkAbstractArray*> components = columnInfo.RawComponentArrays;

    if (valueTypeId == VTK_VOID)
      {
      // schema is not defined or no valid column type is defined for column
      valueTypeId = VTK_STRING;
      }
    if (valueTypeId == VTK_STRING)
      {
      if (components.size() > 0)
        {
        vtkAbstractArray* columnArray = components[0];
        if (columnArray)
          {
          columnArray->SetName(columnName.c_str());
          table->AddColumn(columnArray);
          }
        }
      }
     else
       {
       // Output column. Can be multi-component
       vtkSmartPointer<vtkDataArray> typedColumn = vtkSmartPointer<vtkDataArray>::Take(vtkDataArray::CreateDataArray(valueTypeId));
       typedColumn->SetName(columnName.c_str());
       typedColumn->SetNumberOfComponents(components.size());
       vtkIdType numberOfTuples = 0;
       for (vtkAbstractArray* componentArray : components)
         {
         if (componentArray == nullptr)
           {
           continue;
           }
         numberOfTuples = std::max(numberOfTuples, componentArray->GetNumberOfTuples());
         }
       typedColumn->SetNumberOfTuples(numberOfTuples);

       int componentIndex = 0;
       for (vtkAbstractArray* componentColumn : components)
         {
         if (componentColumn == nullptr)
           {
           continue;
           }

         vtkStringArray* rawColumn = vtkStringArray::SafeDownCast(componentColumn);
         if (!rawColumn)
           {
           continue;
           }

         // Single-component array for a potentially multi-component column
         vtkSmartPointer<vtkDataArray> componentArray = vtkSmartPointer<vtkDataArray>::Take(vtkDataArray::CreateDataArray(valueTypeId));
         componentArray->SetName(rawColumn->GetName());
         componentArray->SetNumberOfComponents(1);
         componentArray->SetNumberOfTuples(numberOfTuples);

         // Initialize with null value
         std::string nullValueStr = tableNode->GetColumnProperty(columnName, "nullValue");
         if (componentArray->IsNumeric())
           {
           // numeric arrays can be initialized in one batch
           double nullValue = 0.0;
           if (!nullValueStr.empty())
             {
             nullValue = vtkVariant(nullValueStr).ToDouble();
             }
           componentArray->FillComponent(0, nullValue);
           }
         else
           {
           vtkVariant nullValue(nullValueStr);
           for (vtkIdType row = 0; row < numberOfTuples; ++row)
             {
             componentArray->SetVariantValue(row, nullValue);
             }
           }

         // Set values
         if (valueTypeId == VTK_CHAR || valueTypeId == VTK_SIGNED_CHAR || valueTypeId == VTK_UNSIGNED_CHAR)
           {
           bool valid = false;
           for (vtkIdType row = 0; row < numberOfTuples; ++row)
             {
             if (rawColumn->GetValue(row).empty())
               {
               // empty cell, leave the null value
               continue;
               }
             int value = componentColumn->GetVariantValue(row).ToInt(&valid);
             if (!valid)
               {
               continue;
               }
             componentArray->SetVariantValue(row, vtkVariant(value));
             }
           }
         else
           {
           for (vtkIdType row = 0; row < numberOfTuples; ++row)
             {
             if (rawColumn->GetValue(row).empty())
               {
               // empty cell, leave the null value
               continue;
               }
             componentArray->SetVariantValue(row, rawColumn->GetVariantValue(row));
             }
           }

         if (components.size() > 1)
           {
           // Multi-component column. Copy the contents of the single component column into the output.
           typedColumn->CopyComponent(componentIndex, componentArray, 0);
           }
         else
           {
           // Single-component column. Add the column directly to the output.
           typedColumn = componentArray;
           }

         if (componentIndex < columnInfo.ComponentNames.size())
           {
           std::string componentName = columnInfo.ComponentNames[componentIndex];
           typedColumn->SetComponentName(componentIndex, componentName.c_str());
           }
         ++componentIndex;
         }

      table->AddColumn(typedColumn);
      }
    }

  tableNode->SetAndObserveTable(table);
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLTableStorageNode::WriteTable(std::string filename, vtkMRMLTableNode* tableNode)
{
  vtkTable* originalTable = tableNode->GetTable();

  vtkNew<vtkTable> newTable;
  for (int i = 0; i < originalTable->GetNumberOfColumns(); ++i)
    {
    vtkAbstractArray* oldColumn = originalTable->GetColumn(i);
    vtkDataArray* oldDataArray = vtkDataArray::SafeDownCast(oldColumn);
    int numberOfComponents = oldColumn->GetNumberOfComponents();
    if (oldDataArray && numberOfComponents > 1)
      {
      std::string columnName;
      if (oldColumn->GetName())
        {
        columnName = oldColumn->GetName();
        }
      for (int componentIndex = 0; componentIndex < numberOfComponents; ++componentIndex)
        {
        std::stringstream newColumnNameSS;
        newColumnNameSS << columnName << "_";
        if (oldColumn->GetComponentName(componentIndex))
          {
          newColumnNameSS << oldColumn->GetComponentName(componentIndex);
          }
        else
          {
          newColumnNameSS << componentIndex;
          }
        std::string newColumnName = newColumnNameSS.str();
        vtkSmartPointer<vtkDataArray> newColumn = vtkSmartPointer<vtkDataArray>::Take(oldDataArray->NewInstance());
        newColumn->SetNumberOfComponents(1);
        newColumn->SetNumberOfTuples(oldColumn->GetNumberOfTuples());
        newColumn->SetName(newColumnName.c_str());
        newColumn->CopyComponent(0, oldDataArray, componentIndex);
        newTable->AddColumn(newColumn);
        }
      }
    else
      {
      newTable->AddColumn(oldColumn);
      }
    }

  vtkNew<vtkDelimitedTextWriter> writer;
  writer->SetFileName(filename.c_str());
  writer->SetInputData(newTable);

  std::string delimiter = this->GetFieldDelimiterCharacters(filename);
  writer->SetFieldDelimiter(delimiter.c_str());

  // SetUseStringDelimiter(true) causes writing each value in double-quotes, which is not very nice,
  // but if the delimiter character is the comma then we have to use this mode, as commas occur in
  // string values quite often.
  writer->SetUseStringDelimiter(delimiter==",");

  vtkNew<vtkErrorSink> errorWarningObserver;
  errorWarningObserver->SetObservedObject(writer);
  try
    {
    writer->Write();
    }
  catch (...)
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::WriteTable: failed to write file: " << filename);
    return false;
    }
  errorWarningObserver->DisplayMessages();
  if (errorWarningObserver->HasErrors())
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::WriteTable: failed to write file: " << filename);
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLTableStorageNode::WriteSchema(std::string filename, vtkMRMLTableNode* tableNode)
{
  vtkNew<vtkTable> schemaTable;

  // Create a copy, as it is not nice if writing to file has a side effect of modifying some
  // data in the node
  if (tableNode->GetSchema())
    {
    schemaTable->DeepCopy(tableNode->GetSchema());
    }

  vtkStringArray* columnNameArray = vtkStringArray::SafeDownCast(schemaTable->GetColumnByName("columnName"));
  if (columnNameArray == nullptr)
    {
    vtkNew<vtkStringArray> newArray;
    newArray->SetName("columnName");
    newArray->SetNumberOfValues(schemaTable->GetNumberOfRows());
    schemaTable->AddColumn(newArray.GetPointer());
    columnNameArray = newArray.GetPointer();
    }

  vtkStringArray* columnTypeArray = vtkStringArray::SafeDownCast(schemaTable->GetColumnByName("type"));
  if (columnTypeArray == nullptr)
    {
    vtkNew<vtkStringArray> newArray;
    newArray->SetName("type");
    newArray->SetNumberOfValues(schemaTable->GetNumberOfRows());
    schemaTable->AddColumn(newArray.GetPointer());
    columnTypeArray = newArray.GetPointer();
    }

  // Add column type to schema
  vtkTable* table = tableNode->GetTable();
  if (table != nullptr)
    {
    for (int col = 0; col < table->GetNumberOfColumns(); ++col)
      {
      vtkAbstractArray* column = table->GetColumn(col);
      if (column == nullptr)
        {
        // invalid column
        continue;
        }
      if (!column->GetName())
        {
        vtkWarningMacro("vtkMRMLTableStorageNode::WriteSchema: empty column name in file: " << filename << ", skipping column");
        continue;
        }

      vtkIdType schemaRowIndex = columnNameArray->LookupValue(column->GetName());
      if (schemaRowIndex < 0)
        {
        schemaRowIndex = schemaTable->InsertNextBlankRow();
        columnNameArray->SetValue(schemaRowIndex, column->GetName());
        }
      columnTypeArray->SetValue(schemaRowIndex, vtkMRMLTableNode::GetValueTypeAsString(column->GetDataType()));
      }
    }

  vtkStringArray* componentNamesArray = vtkStringArray::SafeDownCast(schemaTable->GetColumnByName("componentNames"));
  if (componentNamesArray == nullptr)
    {
    vtkNew<vtkStringArray> newArray;
    newArray->SetName("componentNames");
    newArray->SetNumberOfValues(schemaTable->GetNumberOfRows());
    schemaTable->AddColumn(newArray.GetPointer());
    componentNamesArray = newArray.GetPointer();
    }

  if (componentNamesArray != nullptr)
    {
    for (int col = 0; col < table->GetNumberOfColumns(); ++col)
      {
      vtkAbstractArray* column = table->GetColumn(col);
      if (column == nullptr)
        {
        // invalid column
        continue;
        }
      if (!column->GetName())
        {
        vtkWarningMacro("vtkMRMLTableStorageNode::WriteSchema: empty column name in file: " << filename << ", skipping column");
        continue;
        }
      vtkIdType schemaRowIndex = columnNameArray->LookupValue(column->GetName());
      if (schemaRowIndex < 0)
        {
        schemaRowIndex = schemaTable->InsertNextBlankRow();
        columnNameArray->SetValue(schemaRowIndex, column->GetName());
        }

      std::stringstream componentNamesSS;
      if (column->GetNumberOfComponents() > 1)
        {
        for (int componentIndex = 0; componentIndex < column->GetNumberOfComponents(); ++componentIndex)
          {
          if (componentIndex != 0)
            {
            componentNamesSS << "|";
            }

          if (column->GetComponentName(componentIndex))
            {
            componentNamesSS << column->GetComponentName(componentIndex);
            }
          else
            {
            componentNamesSS << componentIndex;
            }
          }
        }
      std::string componentNames = componentNamesSS.str();
      componentNamesArray->SetValue(schemaRowIndex, componentNames.c_str());
      }
    }

  vtkNew<vtkDelimitedTextWriter> writer;
  vtkNew<vtkErrorSink> errorWarningObserver;
  errorWarningObserver->SetObservedObject(writer);

  writer->SetFileName(filename.c_str());
  writer->SetInputData(schemaTable.GetPointer());

  std::string delimiter = this->GetFieldDelimiterCharacters(filename);
  writer->SetFieldDelimiter(delimiter.c_str());

  // SetUseStringDelimiter(true) causes writing each value in double-quotes, which is not very nice,
  // but if the delimiter character is the comma then we have to use this mode, as commas occur in
  // string values quite often.
  writer->SetUseStringDelimiter(delimiter == ",");

  try
    {
    writer->Write();
    }
  catch (...)
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::WriteSchema: failed to write file: " << filename);
    return false;
    }

  errorWarningObserver->DisplayMessages();
  if (errorWarningObserver->HasErrors())
    {
    vtkErrorMacro("vtkMRMLTableStorageNode::WriteTable: failed to write file: " << filename);
    return false;
    }

  return true;
}
