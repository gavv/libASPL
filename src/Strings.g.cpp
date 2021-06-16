// THIS FILE IS AUTO-GENERATED. DO NOT EDIT!

// Generator: generate-strings.py
// Source: CoreAudio/AudioServerPlugIn.h

// Copyright (c) libASPL authors
// Licensed under MIT

#include "Strings.hpp"

namespace aspl {

std::string ClassIDToString(AudioClassID classID)
{
    switch (classID) {
    case kAudioBooleanControlClassID:
        return "kAudioBooleanControlClassID";
    case kAudioBoxClassID:
        return "kAudioBoxClassID";
    case kAudioClipLightControlClassID:
        return "kAudioClipLightControlClassID";
    case kAudioClockDeviceClassID:
        return "kAudioClockDeviceClassID";
    case kAudioClockSourceControlClassID:
        return "kAudioClockSourceControlClassID";
    case kAudioControlClassID:
        return "kAudioControlClassID";
    case kAudioDataDestinationControlClassID:
        return "kAudioDataDestinationControlClassID";
    case kAudioDataSourceControlClassID:
        return "kAudioDataSourceControlClassID";
    case kAudioDeviceClassID:
        return "kAudioDeviceClassID";
    case kAudioEndPointClassID:
        return "kAudioEndPointClassID";
    case kAudioEndPointDeviceClassID:
        return "kAudioEndPointDeviceClassID";
    case kAudioHighPassFilterControlClassID:
        return "kAudioHighPassFilterControlClassID";
    case kAudioJackControlClassID:
        return "kAudioJackControlClassID";
    case kAudioLFEMuteControlClassID:
        return "kAudioLFEMuteControlClassID";
    case kAudioLFEVolumeControlClassID:
        return "kAudioLFEVolumeControlClassID";
    case kAudioLevelControlClassID:
        return "kAudioLevelControlClassID";
    case kAudioLineLevelControlClassID:
        return "kAudioLineLevelControlClassID";
    case kAudioListenbackControlClassID:
        return "kAudioListenbackControlClassID";
    case kAudioMuteControlClassID:
        return "kAudioMuteControlClassID";
    case kAudioObjectClassID:
        return "kAudioObjectClassID";
    case kAudioPhantomPowerControlClassID:
        return "kAudioPhantomPowerControlClassID";
    case kAudioPhaseInvertControlClassID:
        return "kAudioPhaseInvertControlClassID";
    case kAudioPlugInClassID:
        return "kAudioPlugInClassID";
    case kAudioSelectorControlClassID:
        return "kAudioSelectorControlClassID";
    case kAudioSliderControlClassID:
        return "kAudioSliderControlClassID";
    case kAudioSoloControlClassID:
        return "kAudioSoloControlClassID";
    case kAudioStereoPanControlClassID:
        return "kAudioStereoPanControlClassID";
    case kAudioStreamClassID:
        return "kAudioStreamClassID";
    case kAudioTalkbackControlClassID:
        return "kAudioTalkbackControlClassID";
    case kAudioTransportManagerClassID:
        return "kAudioTransportManagerClassID";
    case kAudioVolumeControlClassID:
        return "kAudioVolumeControlClassID";
    default:
        return CodeToString(classID);
    }
}

std::string PropertySelectorToString(AudioObjectPropertySelector selector)
{
    switch (selector) {
    case kAudioBooleanControlPropertyValue:
        return "kAudioBooleanControlPropertyValue";
    case kAudioBoxPropertyAcquired:
        return "kAudioBoxPropertyAcquired";
    case kAudioBoxPropertyAcquisitionFailed:
        return "kAudioBoxPropertyAcquisitionFailed";
    case kAudioBoxPropertyBoxUID:
        return "kAudioBoxPropertyBoxUID";
    case kAudioBoxPropertyClockDeviceList:
        return "kAudioBoxPropertyClockDeviceList";
    case kAudioBoxPropertyDeviceList:
        return "kAudioBoxPropertyDeviceList";
    case kAudioBoxPropertyHasAudio:
        return "kAudioBoxPropertyHasAudio";
    case kAudioBoxPropertyHasMIDI:
        return "kAudioBoxPropertyHasMIDI";
    case kAudioBoxPropertyHasVideo:
        return "kAudioBoxPropertyHasVideo";
    case kAudioBoxPropertyIsProtected:
        return "kAudioBoxPropertyIsProtected";
    case kAudioClockDevicePropertyDeviceUID:
        return "kAudioClockDevicePropertyDeviceUID";
    case kAudioControlPropertyElement:
        return "kAudioControlPropertyElement";
    case kAudioControlPropertyScope:
        return "kAudioControlPropertyScope";
    case kAudioDevicePropertyAvailableNominalSampleRates:
        return "kAudioDevicePropertyAvailableNominalSampleRates";
    case kAudioDevicePropertyClockAlgorithm:
        return "kAudioDevicePropertyClockAlgorithm";
    case kAudioDevicePropertyClockDomain:
        return "kAudioDevicePropertyClockDomain";
    case kAudioDevicePropertyClockIsStable:
        return "kAudioDevicePropertyClockIsStable";
    case kAudioDevicePropertyConfigurationApplication:
        return "kAudioDevicePropertyConfigurationApplication";
    case kAudioDevicePropertyDeviceCanBeDefaultDevice:
        return "kAudioDevicePropertyDeviceCanBeDefaultDevice";
    case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
        return "kAudioDevicePropertyDeviceCanBeDefaultSystemDevice";
    case kAudioDevicePropertyDeviceIsAlive:
        return "kAudioDevicePropertyDeviceIsAlive";
    case kAudioDevicePropertyDeviceIsRunning:
        return "kAudioDevicePropertyDeviceIsRunning";
    case kAudioDevicePropertyIcon:
        return "kAudioDevicePropertyIcon";
    case kAudioDevicePropertyIsHidden:
        return "kAudioDevicePropertyIsHidden";
    case kAudioDevicePropertyLatency:
        return "kAudioDevicePropertyLatency";
    case kAudioDevicePropertyModelUID:
        return "kAudioDevicePropertyModelUID";
    case kAudioDevicePropertyNominalSampleRate:
        return "kAudioDevicePropertyNominalSampleRate";
    case kAudioDevicePropertyPreferredChannelLayout:
        return "kAudioDevicePropertyPreferredChannelLayout";
    case kAudioDevicePropertyPreferredChannelsForStereo:
        return "kAudioDevicePropertyPreferredChannelsForStereo";
    case kAudioDevicePropertyRelatedDevices:
        return "kAudioDevicePropertyRelatedDevices";
    case kAudioDevicePropertySafetyOffset:
        return "kAudioDevicePropertySafetyOffset";
    case kAudioDevicePropertyStreams:
        return "kAudioDevicePropertyStreams";
    case kAudioDevicePropertyTransportType:
        return "kAudioDevicePropertyTransportType";
    case kAudioDevicePropertyZeroTimeStampPeriod:
        return "kAudioDevicePropertyZeroTimeStampPeriod";
    case kAudioEndPointDevicePropertyComposition:
        return "kAudioEndPointDevicePropertyComposition";
    case kAudioEndPointDevicePropertyEndPointList:
        return "kAudioEndPointDevicePropertyEndPointList";
    case kAudioEndPointDevicePropertyIsPrivate:
        return "kAudioEndPointDevicePropertyIsPrivate";
    case kAudioLevelControlPropertyConvertDecibelsToScalar:
        return "kAudioLevelControlPropertyConvertDecibelsToScalar";
    case kAudioLevelControlPropertyConvertScalarToDecibels:
        return "kAudioLevelControlPropertyConvertScalarToDecibels";
    case kAudioLevelControlPropertyDecibelRange:
        return "kAudioLevelControlPropertyDecibelRange";
    case kAudioLevelControlPropertyDecibelValue:
        return "kAudioLevelControlPropertyDecibelValue";
    case kAudioLevelControlPropertyScalarValue:
        return "kAudioLevelControlPropertyScalarValue";
    case kAudioObjectPropertyBaseClass:
        return "kAudioObjectPropertyBaseClass";
    case kAudioObjectPropertyClass:
        return "kAudioObjectPropertyClass";
    case kAudioObjectPropertyControlList:
        return "kAudioObjectPropertyControlList";
    case kAudioObjectPropertyCustomPropertyInfoList:
        return "kAudioObjectPropertyCustomPropertyInfoList";
    case kAudioObjectPropertyFirmwareVersion:
        return "kAudioObjectPropertyFirmwareVersion";
    case kAudioObjectPropertyIdentify:
        return "kAudioObjectPropertyIdentify";
    case kAudioObjectPropertyManufacturer:
        return "kAudioObjectPropertyManufacturer";
    case kAudioObjectPropertyModelName:
        return "kAudioObjectPropertyModelName";
    case kAudioObjectPropertyName:
        return "kAudioObjectPropertyName";
    case kAudioObjectPropertyOwnedObjects:
        return "kAudioObjectPropertyOwnedObjects";
    case kAudioObjectPropertyOwner:
        return "kAudioObjectPropertyOwner";
    case kAudioObjectPropertySelectorWildcard:
        return "kAudioObjectPropertySelectorWildcard";
    case kAudioObjectPropertySerialNumber:
        return "kAudioObjectPropertySerialNumber";
    case kAudioPlugInPropertyBoxList:
        return "kAudioPlugInPropertyBoxList";
    case kAudioPlugInPropertyBundleID:
        return "kAudioPlugInPropertyBundleID";
    case kAudioPlugInPropertyClockDeviceList:
        return "kAudioPlugInPropertyClockDeviceList";
    case kAudioPlugInPropertyDeviceList:
        return "kAudioPlugInPropertyDeviceList";
    case kAudioPlugInPropertyResourceBundle:
        return "kAudioPlugInPropertyResourceBundle";
    case kAudioPlugInPropertyTranslateUIDToBox:
        return "kAudioPlugInPropertyTranslateUIDToBox";
    case kAudioPlugInPropertyTranslateUIDToClockDevice:
        return "kAudioPlugInPropertyTranslateUIDToClockDevice";
    case kAudioPlugInPropertyTranslateUIDToDevice:
        return "kAudioPlugInPropertyTranslateUIDToDevice";
    case kAudioSelectorControlPropertyAvailableItems:
        return "kAudioSelectorControlPropertyAvailableItems";
    case kAudioSelectorControlPropertyCurrentItem:
        return "kAudioSelectorControlPropertyCurrentItem";
    case kAudioSelectorControlPropertyItemKind:
        return "kAudioSelectorControlPropertyItemKind";
    case kAudioSelectorControlPropertyItemName:
        return "kAudioSelectorControlPropertyItemName";
    case kAudioSliderControlPropertyRange:
        return "kAudioSliderControlPropertyRange";
    case kAudioSliderControlPropertyValue:
        return "kAudioSliderControlPropertyValue";
    case kAudioStereoPanControlPropertyPanningChannels:
        return "kAudioStereoPanControlPropertyPanningChannels";
    case kAudioStereoPanControlPropertyValue:
        return "kAudioStereoPanControlPropertyValue";
    case kAudioStreamPropertyAvailablePhysicalFormats:
        return "kAudioStreamPropertyAvailablePhysicalFormats";
    case kAudioStreamPropertyAvailableVirtualFormats:
        return "kAudioStreamPropertyAvailableVirtualFormats";
    case kAudioStreamPropertyDirection:
        return "kAudioStreamPropertyDirection";
    case kAudioStreamPropertyIsActive:
        return "kAudioStreamPropertyIsActive";
    case kAudioStreamPropertyStartingChannel:
        return "kAudioStreamPropertyStartingChannel";
    case kAudioStreamPropertyTerminalType:
        return "kAudioStreamPropertyTerminalType";
    case kAudioStreamPropertyVirtualFormat:
        return "kAudioStreamPropertyVirtualFormat";
    case kAudioTransportManagerPropertyEndPointList:
        return "kAudioTransportManagerPropertyEndPointList";
    case kAudioTransportManagerPropertyTranslateUIDToEndPoint:
        return "kAudioTransportManagerPropertyTranslateUIDToEndPoint";
    default:
        return CodeToString(selector);
    }
}

std::string PropertyScopeToString(AudioObjectPropertyScope scope)
{
    switch (scope) {
    case kAudioObjectPropertyScopeGlobal:
        return "kAudioObjectPropertyScopeGlobal";
    case kAudioObjectPropertyScopeInput:
        return "kAudioObjectPropertyScopeInput";
    case kAudioObjectPropertyScopeOutput:
        return "kAudioObjectPropertyScopeOutput";
    case kAudioObjectPropertyScopePlayThrough:
        return "kAudioObjectPropertyScopePlayThrough";
    case kAudioObjectPropertyScopeWildcard:
        return "kAudioObjectPropertyScopeWildcard";
    default:
        return CodeToString(scope);
    }
}

std::string OperationIDToString(UInt32 operationID)
{
    switch (operationID) {
    case kAudioServerPlugInIOOperationConvertInput:
        return "kAudioServerPlugInIOOperationConvertInput";
    case kAudioServerPlugInIOOperationConvertMix:
        return "kAudioServerPlugInIOOperationConvertMix";
    case kAudioServerPlugInIOOperationCycle:
        return "kAudioServerPlugInIOOperationCycle";
    case kAudioServerPlugInIOOperationMixOutput:
        return "kAudioServerPlugInIOOperationMixOutput";
    case kAudioServerPlugInIOOperationProcessInput:
        return "kAudioServerPlugInIOOperationProcessInput";
    case kAudioServerPlugInIOOperationProcessMix:
        return "kAudioServerPlugInIOOperationProcessMix";
    case kAudioServerPlugInIOOperationProcessOutput:
        return "kAudioServerPlugInIOOperationProcessOutput";
    case kAudioServerPlugInIOOperationReadInput:
        return "kAudioServerPlugInIOOperationReadInput";
    case kAudioServerPlugInIOOperationThread:
        return "kAudioServerPlugInIOOperationThread";
    case kAudioServerPlugInIOOperationWriteMix:
        return "kAudioServerPlugInIOOperationWriteMix";
    default:
        return CodeToString(operationID);
    }
}

std::string StatusToString(OSStatus status)
{
    switch (status) {
    case kAudioHardwareNoError:
        return "OK";
    case kAudioHardwareBadDeviceError:
        return "kAudioHardwareBadDeviceError";
    case kAudioHardwareBadObjectError:
        return "kAudioHardwareBadObjectError";
    case kAudioHardwareBadPropertySizeError:
        return "kAudioHardwareBadPropertySizeError";
    case kAudioHardwareBadStreamError:
        return "kAudioHardwareBadStreamError";
    case kAudioHardwareIllegalOperationError:
        return "kAudioHardwareIllegalOperationError";
    case kAudioHardwareNotRunningError:
        return "kAudioHardwareNotRunningError";
    case kAudioHardwareUnknownPropertyError:
        return "kAudioHardwareUnknownPropertyError";
    case kAudioHardwareUnspecifiedError:
        return "kAudioHardwareUnspecifiedError";
    case kAudioHardwareUnsupportedOperationError:
        return "kAudioHardwareUnsupportedOperationError";
    default:
        return CodeToString(UInt32(status));
    }
}

std::string FormatIDToString(AudioFormatID formatID)
{
    switch (formatID) {
    case kAudioFormat60958AC3:
        return "kAudioFormat60958AC3";
    case kAudioFormatAC3:
        return "kAudioFormatAC3";
    case kAudioFormatAES3:
        return "kAudioFormatAES3";
    case kAudioFormatALaw:
        return "kAudioFormatALaw";
    case kAudioFormatAMR:
        return "kAudioFormatAMR";
    case kAudioFormatAMR_WB:
        return "kAudioFormatAMR_WB";
    case kAudioFormatAppleIMA4:
        return "kAudioFormatAppleIMA4";
    case kAudioFormatAppleLossless:
        return "kAudioFormatAppleLossless";
    case kAudioFormatAudible:
        return "kAudioFormatAudible";
    case kAudioFormatDVIIntelIMA:
        return "kAudioFormatDVIIntelIMA";
    case kAudioFormatEnhancedAC3:
        return "kAudioFormatEnhancedAC3";
    case kAudioFormatFLAC:
        return "kAudioFormatFLAC";
    case kAudioFormatLinearPCM:
        return "kAudioFormatLinearPCM";
    case kAudioFormatMACE3:
        return "kAudioFormatMACE3";
    case kAudioFormatMACE6:
        return "kAudioFormatMACE6";
    case kAudioFormatMIDIStream:
        return "kAudioFormatMIDIStream";
    case kAudioFormatMPEG4AAC:
        return "kAudioFormatMPEG4AAC";
    case kAudioFormatMPEG4AAC_ELD:
        return "kAudioFormatMPEG4AAC_ELD";
    case kAudioFormatMPEG4AAC_ELD_SBR:
        return "kAudioFormatMPEG4AAC_ELD_SBR";
    case kAudioFormatMPEG4AAC_ELD_V2:
        return "kAudioFormatMPEG4AAC_ELD_V2";
    case kAudioFormatMPEG4AAC_HE:
        return "kAudioFormatMPEG4AAC_HE";
    case kAudioFormatMPEG4AAC_HE_V2:
        return "kAudioFormatMPEG4AAC_HE_V2";
    case kAudioFormatMPEG4AAC_LD:
        return "kAudioFormatMPEG4AAC_LD";
    case kAudioFormatMPEG4AAC_Spatial:
        return "kAudioFormatMPEG4AAC_Spatial";
    case kAudioFormatMPEG4CELP:
        return "kAudioFormatMPEG4CELP";
    case kAudioFormatMPEG4HVXC:
        return "kAudioFormatMPEG4HVXC";
    case kAudioFormatMPEG4TwinVQ:
        return "kAudioFormatMPEG4TwinVQ";
    case kAudioFormatMPEGD_USAC:
        return "kAudioFormatMPEGD_USAC";
    case kAudioFormatMPEGLayer1:
        return "kAudioFormatMPEGLayer1";
    case kAudioFormatMPEGLayer2:
        return "kAudioFormatMPEGLayer2";
    case kAudioFormatMPEGLayer3:
        return "kAudioFormatMPEGLayer3";
    case kAudioFormatMicrosoftGSM:
        return "kAudioFormatMicrosoftGSM";
    case kAudioFormatOpus:
        return "kAudioFormatOpus";
    case kAudioFormatParameterValueStream:
        return "kAudioFormatParameterValueStream";
    case kAudioFormatQDesign:
        return "kAudioFormatQDesign";
    case kAudioFormatQDesign2:
        return "kAudioFormatQDesign2";
    case kAudioFormatQUALCOMM:
        return "kAudioFormatQUALCOMM";
    case kAudioFormatTimeCode:
        return "kAudioFormatTimeCode";
    case kAudioFormatULaw:
        return "kAudioFormatULaw";
    case kAudioFormatiLBC:
        return "kAudioFormatiLBC";
    default:
        return CodeToString(formatID);
    }
}

std::string FormatFlagsToString(AudioFormatFlags formatFlags)
{
    std::string ret;
    if (formatFlags & kAudioFormatFlagIsAlignedHigh) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsAlignedHigh";
    }
    if (formatFlags & kAudioFormatFlagIsBigEndian) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsBigEndian";
    }
    if (formatFlags & kAudioFormatFlagIsFloat) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsFloat";
    }
    if (formatFlags & kAudioFormatFlagIsNonInterleaved) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsNonInterleaved";
    }
    if (formatFlags & kAudioFormatFlagIsNonMixable) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsNonMixable";
    }
    if (formatFlags & kAudioFormatFlagIsPacked) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsPacked";
    }
    if (formatFlags & kAudioFormatFlagIsSignedInteger) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagIsSignedInteger";
    }
    if (formatFlags & kAudioFormatFlagsAreAllClear) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagsAreAllClear";
    }
    if (formatFlags & kAudioFormatFlagsNativeEndian) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagsNativeEndian";
    }
    if (formatFlags & kAudioFormatFlagsNativeFloatPacked) {
        if (!ret.empty()) {
            ret += "|";
        }
        ret += "kAudioFormatFlagsNativeFloatPacked";
    }
    return ret;
}

} // namespace aspl
