#include "utility.h"

#include <Python.h>  // this has to be the first included header
#include <iostream>
#include <fstream>
#include <sstream>
#include "gtest/gtest.h"
#include "easylogging++.h"

double parseNumber(std::string::iterator &iterFileContents, std::string::iterator iterFileContentsEnd)
{
  // parse number in file Contents
  std::string numberFileContents;
  while ((isdigit(*iterFileContents) || *iterFileContents == '.') && iterFileContents != iterFileContentsEnd)
  {
    numberFileContents += *iterFileContents;
    iterFileContents++;
  }
  double number = stod(numberFileContents, nullptr);
  return number;
}

void assertFileMatchesContent(std::string filename, std::string referenceContents, std::string referenceContents2)
{
  // read in generated exnode file 
  std::ifstream file(filename);

  ASSERT_TRUE(file.is_open()) << "could not open output file"; 
  
  // reserve memory of size of file
  file.seekg(0, std::ios::end);   
  size_t fileSize = file.tellg();
  std::string fileContents(fileSize, ' ');
  
  // reset file pointer
  file.seekg(0, std::ios::beg);
  
  // read in file contents
  file.read(&fileContents[0], fileSize);
  
  bool referenceContentMatches = true;
  bool referenceContent2Matches = true;
  std::stringstream msg;
  
  // iterate through file contents and compare against referenceContents
  std::string::iterator iterReferenceContents = referenceContents.begin(); 
  for (std::string::iterator iterFileContents = fileContents.begin(); iterFileContents != fileContents.end() && iterReferenceContents != referenceContents.end();)
  {
    if (isdigit(*iterFileContents))
    {
      double numberFileContents = parseNumber(iterFileContents, fileContents.end());
      double numberReferenceContents = parseNumber(iterReferenceContents, referenceContents2.end());
      if (fabs(numberFileContents - numberReferenceContents) >= 1e-13)
      {
        referenceContentMatches = false;
        msg << "file content of file \"" << filename << "\" is different (referenceContents). The number " << numberFileContents << " in the file does not match reference value " << numberReferenceContents << ", difference " << fabs(numberFileContents - numberReferenceContents) << std::endl;
      }
    }
    else
    {
      if(*iterFileContents != *iterReferenceContents)
      {
        referenceContentMatches = false;
      }
      iterFileContents++;
      iterReferenceContents++;
    }
  }
  
  if (referenceContents2 != "-")
  {
    // iterate through file contents and compare against referenceContents2
    iterReferenceContents = referenceContents2.begin(); 
    for (std::string::iterator iterFileContents = fileContents.begin(); iterFileContents != fileContents.end() && iterReferenceContents != referenceContents2.end();)
    {
      if (isdigit(*iterFileContents))
      {
        double numberFileContents = parseNumber(iterFileContents, fileContents.end());
        double numberReferenceContents = parseNumber(iterReferenceContents, referenceContents2.end());
        if (fabs(numberFileContents - numberReferenceContents) >= 1e-13) 
        {
          referenceContent2Matches = false;
          msg << "file content of file \"" << filename << "\" is different (referenceContents2). The number " << numberFileContents << " in the file does not match reference value " << numberReferenceContents << ", difference " << fabs(numberFileContents - numberReferenceContents) << std::endl;
        }
      }
      else
      {
        if (*iterFileContents, *iterReferenceContents)
        {
          referenceContent2Matches = false;
        }
        iterFileContents++;
        iterReferenceContents++;
      }
    }
  }
  
  if (!referenceContentMatches && !referenceContent2Matches)
  {
    if (!referenceContentMatches)
      LOG(INFO) << "file content of file \"" << filename << "\" is different (referenceContents). fileContents: " << std::endl << fileContents << std::endl << ", referenceContents: " << std::endl << referenceContents;
    if (!referenceContent2Matches)
      LOG(INFO) << "file content of file \"" << filename << "\" is different (referenceContents2). fileContents: " << std::endl << fileContents << std::endl << ", referenceContents2: " << std::endl << referenceContents2;
    
    LOG(INFO) << msg.str();
    ASSERT_TRUE(false) << "neither referenceContent nor referenceContent2 matches!";
  }
  
}
