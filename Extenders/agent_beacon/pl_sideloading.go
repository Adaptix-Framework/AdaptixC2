package main

import (
 "debug/pe"
 "encoding/binary"
 "fmt"
 "os"
 "path/filepath"
 "strings"
 "unsafe"
)

func CreateDefinitionFile(dllPath string, tempDir string) (string, error) {
 file, err := pe.Open(dllPath)
 if err != nil {
  return "", fmt.Errorf("Failed to open DLL: %v", err)
 }
 defer file.Close()

 exports, err := getExports(file)
 if err != nil {
  return "", fmt.Errorf("Failed to read exports: %v", err)
 }
 if len(exports) == 0 {
  return "", fmt.Errorf("No exports found in DLL")
 }

 baseName := strings.TrimSuffix(filepath.Base(dllPath), filepath.Ext(dllPath))
 defFileName := tempDir + "/" + baseName + ".def"
 defFile, err := os.Create(defFileName)
 if err != nil {
  return "", fmt.Errorf("Failed to create def file: %v", err)
 }
 defer defFile.Close()

 fmt.Fprintf(defFile, "EXPORTS\n")
 for i, exportName := range exports {
	 fmt.Fprintf(defFile, "%s=%s.%s @%v\n", exportName, baseName, exportName, i+1)
 }

 //fmt.Printf("Definition file written to: %s\n", defFileName)
 return defFileName, nil
}

type ExportDirectory struct {
 ExportFlags       uint32
 TimeDateStamp     uint32
 MajorVersion      uint16
 MinorVersion      uint16
 NameRVA           uint32
 OrdinalBase       uint32
 NumberOfFunctions uint32
 NumberOfNames     uint32
 AddressTableRVA   uint32
 NamePointerRVA    uint32
 OrdinalTableRVA   uint32
}

func getExports(file *pe.File) ([]string, error) {
 var exports []string
 
 // Get the export directory from the data directories
 if file.OptionalHeader == nil {
  return nil, fmt.Errorf("no optional header found")
 }
 
 var exportDirRVA, exportDirSize uint32
 
 switch oh := file.OptionalHeader.(type) {
 case *pe.OptionalHeader32:
  if len(oh.DataDirectory) > 0 {
   exportDirRVA = oh.DataDirectory[0].VirtualAddress
   exportDirSize = oh.DataDirectory[0].Size
  }
 case *pe.OptionalHeader64:
  if len(oh.DataDirectory) > 0 {
   exportDirRVA = oh.DataDirectory[0].VirtualAddress
   exportDirSize = oh.DataDirectory[0].Size
  }
 default:
  return nil, fmt.Errorf("unsupported optional header type")
 }
 
 if exportDirRVA == 0 || exportDirSize == 0 {
  return nil, fmt.Errorf("no export directory found")
 }
 
 // Find the section containing the export directory
 var section *pe.Section
 for _, s := range file.Sections {
  if exportDirRVA >= s.VirtualAddress && exportDirRVA < s.VirtualAddress+s.VirtualSize {
   section = s
   break
  }
 }
 
 if section == nil {
  return nil, fmt.Errorf("export directory not found in any section")
 }
 
 // Read section data
 data, err := section.Data()
 if err != nil {
  return nil, fmt.Errorf("failed to read section data: %v", err)
 }
 
 // Calculate offset within section
 offset := exportDirRVA - section.VirtualAddress
 if offset >= uint32(len(data)) {
  return nil, fmt.Errorf("export directory offset out of bounds")
 }
 
 // Parse export directory
 if len(data[offset:]) < int(unsafe.Sizeof(ExportDirectory{})) {
  return nil, fmt.Errorf("insufficient data for export directory")
 }
 
 exportDir := (*ExportDirectory)(unsafe.Pointer(&data[offset]))
 
 if exportDir.NumberOfNames == 0 {
  return nil, fmt.Errorf("no named exports found")
 }
 
 // Read name pointer table
 nameTableRVA := exportDir.NamePointerRVA
 nameTableSection := findSectionByRVA(file, nameTableRVA)
 if nameTableSection == nil {
  return nil, fmt.Errorf("name table section not found")
 }
 
 nameTableData, err := nameTableSection.Data()
 if err != nil {
  return nil, fmt.Errorf("failed to read name table section: %v", err)
 }
 
 nameTableOffset := nameTableRVA - nameTableSection.VirtualAddress
 
 // Read each name RVA and then the actual name
 for i := uint32(0); i < exportDir.NumberOfNames; i++ {
  nameRVAOffset := nameTableOffset + i*4
  if nameRVAOffset+4 > uint32(len(nameTableData)) {
   break
  }
  
  nameRVA := binary.LittleEndian.Uint32(nameTableData[nameRVAOffset:])
  
  // Find section containing the name
  nameSection := findSectionByRVA(file, nameRVA)
  if nameSection == nil {
   continue
  }
  
  nameSectionData, err := nameSection.Data()
  if err != nil {
   continue
  }
  
  nameOffset := nameRVA - nameSection.VirtualAddress
  if nameOffset >= uint32(len(nameSectionData)) {
   continue
  }
  
  // Read null-terminated string
  nameBytes := nameSectionData[nameOffset:]
  var name string
  for j, b := range nameBytes {
   if b == 0 {
    name = string(nameBytes[:j])
    break
   }
  }
  
  if name != "" {
   exports = append(exports, name)
  }
 }
 
 return exports, nil
}

func findSectionByRVA(file *pe.File, rva uint32) *pe.Section {
 for _, section := range file.Sections {
  if rva >= section.VirtualAddress && rva < section.VirtualAddress+section.VirtualSize {
   return section
  }
 }
 return nil
}
