/* <auto_header>
 * <FILENAME>
 * 
 * INTEL CONFIDENTIAL
 * Copyright © 2011 Intel 
 * Corporation All Rights Reserved.
 * 
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel’s prior express written permission.
 * 
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 * 
 *  AUTHOR: Patrick Benavoli (patrickx.benavoli@intel.com)
 * CREATED: 2011-06-01
 * UPDATED: 2011-07-27
 * 
 * 
 * </auto_header>
 */
#include "ArrayParameter.h"
#include <sstream> // for istringstream
#include "Tokenizer.h"
#include "ParameterType.h"
#include "ParameterAccessContext.h"
#include "ConfigurationAccessContext.h"
#include "ParameterBlackboard.h"
#include <assert.h>

#define base CParameter

CArrayParameter::CArrayParameter(const string& strName, const CTypeElement* pTypeElement) : base(strName, pTypeElement)
{
}

uint32_t CArrayParameter::getFootPrint() const
{
    return getSize() * getArrayLength();
}

// Array length
uint32_t CArrayParameter::getArrayLength() const
{
    return getTypeElement()->getArrayLength();
}

// Element properties
void CArrayParameter::showProperties(string& strResult) const
{
    base::showProperties(strResult);

    // Array length
    strResult += "Array length: ";
    strResult += toString(getArrayLength());
    strResult += "\n";
}

// XML configuration settings parsing
bool CArrayParameter::serializeXmlSettings(CXmlElement& xmlConfigurationSettingsElementContent, CConfigurationAccessContext& configurationAccessContext) const
{
    // Check for value space
    handleValueSpaceAttribute(xmlConfigurationSettingsElementContent, configurationAccessContext);

    // Handle access
    if (!configurationAccessContext.serializeOut()) {

        // Actually set values to blackboard
        if (!setValues(0, configurationAccessContext.getBaseOffset(), xmlConfigurationSettingsElementContent.getTextContent(), configurationAccessContext)) {

            return false;
        }
    } else {

        // Get string value
        string strValue;

        // Whole array requested
        getValues(configurationAccessContext.getBaseOffset(), strValue, configurationAccessContext);

        // Populate value into xml text node
        xmlConfigurationSettingsElementContent.setTextContent(strValue);
    }

    // Done
    return true;
}

// User set/get
bool CArrayParameter::accessValue(CPathNavigator& pathNavigator, string& strValue, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    uint32_t uiIndex;

    if (!getIndex(pathNavigator, uiIndex, parameterAccessContext)) {

        return false;
    }

    if (bSet) {
        // Set
        if (uiIndex == (uint32_t)-1) {

            // No index provided, start with 0
            uiIndex = 0;
        }

        // Actually set values
        if (!setValues(uiIndex, 0, strValue, parameterAccessContext)) {

            return false;
        }

        // Synchronize
        if (parameterAccessContext.getAutoSync() && !sync(parameterAccessContext)) {

            // Append parameter path to error
            parameterAccessContext.appendToError(" " + getPath());

            return false;
        }
    } else {
        // Get
        if (uiIndex == (uint32_t)-1) {

            // Whole array requested
            getValues(0, strValue, parameterAccessContext);

        } else {
            // Scalar requested
            doGetValue(strValue, getOffset() + uiIndex * getSize(), parameterAccessContext);
        }
    }

    return true;
}

// Boolean
bool CArrayParameter::accessAsBooleanArray(vector<bool>& abValues, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    return accessValues(abValues, bSet, parameterAccessContext);
}

// Integer
bool CArrayParameter::accessAsIntegerArray(vector<uint32_t>& auiValues, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    return accessValues(auiValues, bSet, parameterAccessContext);
}

// Signed Integer Access
bool CArrayParameter::accessAsSignedIntegerArray(vector<int32_t>& aiValues, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    return accessValues(aiValues, bSet, parameterAccessContext);
}

// Double Access
bool CArrayParameter::accessAsDoubleArray(vector<double>& adValues, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    return accessValues(adValues, bSet, parameterAccessContext);
}

// String Access
bool CArrayParameter::accessAsStringArray(vector<string>& astrValues, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    return accessValues(astrValues, bSet, parameterAccessContext);
}

// Dump
void CArrayParameter::logValue(string& strValue, CErrorContext& errorContext) const
{
    // Parameter context
    CParameterAccessContext& parameterAccessContext = static_cast<CParameterAccessContext&>(errorContext);

    // Dump values
    getValues(0, strValue, parameterAccessContext);
}

// Used for simulation and virtual subsystems
void CArrayParameter::setDefaultValues(CParameterAccessContext& parameterAccessContext) const
{
    // Get default value from type
    uint32_t uiDefaultValue = static_cast<const CParameterType*>(getTypeElement())->getDefaultValue();

    // Write blackboard
    CParameterBlackboard* pBlackboard = parameterAccessContext.getParameterBlackboard();

    // Process
    uint32_t uiValueIndex;
    uint32_t uiSize = getSize();
    uint32_t uiOffset = getOffset();
    bool bSubsystemIsBigEndian = parameterAccessContext.isBigEndianSubsystem();
    uint32_t uiArrayLength = getArrayLength();

    for (uiValueIndex = 0; uiValueIndex < uiArrayLength; uiValueIndex++) {

        // Beware this code works on little endian architectures only!
        pBlackboard->writeInteger(&uiDefaultValue, uiSize, uiOffset, bSubsystemIsBigEndian);

        uiOffset += uiSize;
    }
}

// Index from path
bool CArrayParameter::getIndex(CPathNavigator& pathNavigator, uint32_t& uiIndex, CParameterAccessContext& parameterAccessContext) const
{
    uiIndex = (uint32_t)-1;

    string* pStrChildName = pathNavigator.next();

    if (pStrChildName) {

        // Check index is numeric
        istringstream iss(*pStrChildName);

        iss >> uiIndex;

        if (!iss) {

            parameterAccessContext.setError("Expected numerical expression as last item in " + pathNavigator.getCurrentPath());

            return false;
        }

        if (uiIndex >= getArrayLength()) {
            ostringstream oss;

            oss << "Provided index out of range (max is " << getArrayLength() - 1 << ")";

            parameterAccessContext.setError(oss.str());

            return false;
        }

        // Check no other item provided in path
        pStrChildName = pathNavigator.next();

        if (pStrChildName) {

            // Should be leaf element
            parameterAccessContext.setError("Path not found: " + pathNavigator.getCurrentPath());

            return false;
        }
    }

    return true;
}

// Common set value processing
bool CArrayParameter::setValues(uint32_t uiStartIndex, uint32_t uiBaseOffset, const string& strValue, CParameterAccessContext& parameterAccessContext) const
{
    // Deal with value(s)
    Tokenizer tok(strValue, DEFAULT_DELIMITER + ",");

    vector<string> astrValues = tok.split();
    uint32_t uiNbValues = astrValues.size();

    // Check number of provided values
    if (uiNbValues + uiStartIndex > getArrayLength()) {

        // Out of bounds
        parameterAccessContext.setError("Too many values provided");

        return false;
    }

    // Process
    uint32_t uiValueIndex;
    uint32_t uiSize = getSize();
    uint32_t uiOffset = getOffset() + uiStartIndex * uiSize - uiBaseOffset;

    for (uiValueIndex = 0; uiValueIndex < uiNbValues; uiValueIndex++) {

        if (!doSetValue(astrValues[uiValueIndex], uiOffset, parameterAccessContext)) {

            // Append parameter path to error
            parameterAccessContext.appendToError(" " + getPath() + "/" + toString(uiValueIndex + uiStartIndex));

            return false;
        }

        uiOffset += uiSize;
    }
    return true;
}

// Common get value processing
void CArrayParameter::getValues(uint32_t uiBaseOffset, string& strValues, CParameterAccessContext& parameterAccessContext) const
{
    uint32_t uiValueIndex;
    uint32_t uiSize = getSize();
    uint32_t uiOffset = getOffset() - uiBaseOffset;
    uint32_t uiArrayLength = getArrayLength();

    strValues.clear();

    bool bFirst = true;

    for (uiValueIndex = 0; uiValueIndex < uiArrayLength; uiValueIndex++) {
        string strReadValue;

        doGetValue(strReadValue, uiOffset, parameterAccessContext);

        if (!bFirst) {

            strValues += " ";
        } else {

            bFirst = false;
        }

        strValues += strReadValue;

        uiOffset += uiSize;
    }
}

// Generic Access
template <typename type>
bool CArrayParameter::accessValues(vector<type>& values, bool bSet, CParameterAccessContext& parameterAccessContext) const
{
    bool bSuccess;

    if (bSet) {

        if  (setValues(values, parameterAccessContext)) {

            // Synchronize
            bSuccess = sync(parameterAccessContext);
        } else {

            bSuccess = false;
        }
    } else {

        bSuccess = getValues(values, parameterAccessContext);
    }
    if (!bSuccess) {

        // Append parameter path to error
        parameterAccessContext.appendToError(" " + getPath());
    }
    return bSuccess;
}

template <typename type>
bool CArrayParameter::setValues(const vector<type>& values, CParameterAccessContext& parameterAccessContext) const
{
    uint32_t uiNbValues = getArrayLength();
    uint32_t uiValueIndex;
    uint32_t uiSize = getSize();
    uint32_t uiOffset = getOffset();

    assert(values.size() == uiNbValues);

    // Process
    for (uiValueIndex = 0; uiValueIndex < uiNbValues; uiValueIndex++) {

        if (!doSet(values[uiValueIndex], uiOffset, parameterAccessContext)) {

            return false;
        }

        uiOffset += uiSize;
    }

   return true;
}

template <typename type>
bool CArrayParameter::getValues(vector<type>& values, CParameterAccessContext& parameterAccessContext) const
{
    uint32_t uiNbValues = getArrayLength();
    uint32_t uiValueIndex;
    uint32_t uiSize = getSize();
    uint32_t uiOffset = getOffset();

    values.clear();

    for (uiValueIndex = 0; uiValueIndex < uiNbValues; uiValueIndex++) {
        type readValue;

        if (!doGet(readValue, uiOffset, parameterAccessContext)) {

            return false;
        }

        values.push_back(readValue);

        uiOffset += uiSize;
    }
    return true;
}

template <typename type>
bool CArrayParameter::doSet(type value, uint32_t uiOffset, CParameterAccessContext& parameterAccessContext) const
{
    uint32_t uiData;

    if (!static_cast<const CParameterType*>(getTypeElement())->toBlackboard(value, uiData, parameterAccessContext)) {

        return false;
    }
    // Write blackboard
    CParameterBlackboard* pBlackboard = parameterAccessContext.getParameterBlackboard();

    // Beware this code works on little endian architectures only!
    pBlackboard->writeInteger(&uiData, getSize(), uiOffset, parameterAccessContext.isBigEndianSubsystem());

    return true;
}

template <typename type>
bool CArrayParameter::doGet(type& value, uint32_t uiOffset, CParameterAccessContext& parameterAccessContext) const
{
    uint32_t uiData = 0;

    // Read blackboard
    const CParameterBlackboard* pBlackboard = parameterAccessContext.getParameterBlackboard();

    // Beware this code works on little endian architectures only!
    pBlackboard->readInteger(&uiData, getSize(), uiOffset, parameterAccessContext.isBigEndianSubsystem());

    return static_cast<const CParameterType*>(getTypeElement())->fromBlackboard(value, uiData, parameterAccessContext);
}

