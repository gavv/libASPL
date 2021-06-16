#! /usr/bin/env python3
import argparse
import builtins
import collections
import jinja2
import json
import os
import sys

class_fields = {
    'class': None,
    'base_class': None,
    'class_tree': [],
    'setter_mutex': None,
    'properties': {}
}

prop_fields = {
    'id': None,
    'type': None,
    'user_type': None,
    'allowed_scopes': None,
    'is_gettable': True,
    'is_settable': False,
    'is_user_settable': False,
    'hand_written_setter': False,
    'scoped_getter': False,
    'scoped_notification': False,
    'is_async': False,
    'is_checked': False,
    'is_array': False,
    'is_truncatable': True,
    'is_converter': False,
    'is_qualified': False,
    'qualifier_type': None,
}

parser = argparse.ArgumentParser()
parser.add_argument('-i', type=str, help='input json file', required=True)
parser.add_argument('-o', type=str, help='output c++ file', required=True)

args = parser.parse_args()

with open(args.i) as json_file:
    klass = json.load(json_file, object_pairs_hook=collections.OrderedDict)

for field in klass.keys():
    if not field in class_fields:
        raise RuntimeError("unknown class field %s" % field)

for prop in klass['properties'].values():
    for field in prop.keys():
        if not field in prop_fields:
            raise RuntimeError("unknown property field %s" % field)

for field, value in class_fields.items():
    if not field in klass:
        klass[field] = value

for prop in klass['properties'].values():
    for field, value in prop_fields.items():
        if not field in prop:
            prop[field] = value

env = jinja2.Environment(
    trim_blocks=True,
    lstrip_blocks=True,
    undefined = jinja2.StrictUndefined)

template = env.from_string('''
// THIS FILE IS AUTO-GENERATED. DO NOT EDIT!

// Generator: {{ generator_script }}
// Source: {{ generator_input }}

// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/{{ class }}.hpp>

#include "Compare.hpp"
#include "Convert.hpp"
#include "Traits.hpp"

#include <algorithm>

namespace aspl {

AudioClassID {{ class }}::GetClass() const
{
    return {{ class_tree[0] }};
}

AudioClassID {{ class }}::GetBaseClass() const
{
    return {{ class_tree[1] if len(class_tree) > 1 else class_tree[0] }};
}

bool {{ class }}::IsInstance(AudioClassID classID) const
{
    switch (classID) {
    {% for class_id in class_tree %}
    case {{ class_id }}:
    {% endfor %}
        return true;
    default:
        return false;
    }
}
{% for prop_name, prop in properties.items() %}
{% if (prop.is_settable or prop.is_user_settable) and not prop.hand_written_setter %}
{% if prop.is_async %}

OSStatus {{ class }}::Set{{ prop_name }}Async({{ prop.user_type or prop.type }} value)
{
    std::lock_guard<decltype({{ setter_mutex }})> writeLock({{ setter_mutex }});

    Tracer::Operation op;
    op.Name = "{{ class }}::Set{{ prop_name }}Async()";
    op.ObjectID = GetID();

    GetContext()->Tracer->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    if (value == Get{{ prop_name }}()) {
        GetContext()->Tracer->Message("value not changed");
        goto end;
    }
{% if prop.is_checked %}

    status = Check{{ prop_name }}(value);
    if (status != kAudioHardwareNoError) {
        GetContext()->Tracer->Message("value is invalid");
        goto end;
    }
{% endif %}

    RequestConfigurationChange([this, value]() {
        std::lock_guard<decltype({{ setter_mutex }})> writeLock({{ setter_mutex }});

        Tracer::Operation op;
        op.Name = "{{ class }}::Set{{ prop_name }}Impl()";
        op.ObjectID = GetID();

        GetContext()->Tracer->OperationBegin(op);

        OSStatus status = kAudioHardwareNoError;

        if (value == Get{{ prop_name }}()) {
            GetContext()->Tracer->Message("value not changed");
        } else {
            GetContext()->Tracer->Message("setting value to %s",
                Convert::ToString(value).c_str());

            status = Set{{ prop_name }}Impl(value);
        }

        GetContext()->Tracer->OperationEnd(op, status);
    });

end:
    GetContext()->Tracer->OperationEnd(op, status);

    return status;
}
{% else %}

OSStatus {{ class }}::Set{{ prop_name }}({{ prop.user_type or prop.type }} value)
{
    std::lock_guard<decltype({{ setter_mutex }})> writeLock({{ setter_mutex }});

    Tracer::Operation op;
    op.Name = "{{ class }}::Set{{ prop_name }}()";
    op.ObjectID = GetID();

    GetContext()->Tracer->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;

    if (value == Get{{ prop_name }}()) {
        GetContext()->Tracer->Message("value not changed");
        goto end;
    }
{% if prop.is_checked %}

    status = Check{{ prop_name }}(value);
    if (status != kAudioHardwareNoError) {
        GetContext()->Tracer->Message("value is invalid");
        goto end;
    }
{% endif %}

    GetContext()->Tracer->Message("setting value to %s",
        Convert::ToString(value).c_str());

    status = Set{{ prop_name }}Impl(value);
    if (status != kAudioHardwareNoError) {
        GetContext()->Tracer->Message("setter failed");
        goto end;
    }

{% if prop.scoped_notification %}
    NotifyPropertyChanged({{ prop.id }}, GetScope(), GetElement());
{% else %}
    NotifyPropertyChanged({{ prop.id }});
{% endif %}

end:
    GetContext()->Tracer->OperationEnd(op, status);

    return status;
}
{% endif %}
{% endif %}
{% endfor %}

Boolean {{ class }}::HasProperty(AudioObjectID objectID,
    pid_t clientPID,
    const AudioObjectPropertyAddress* address) const
{
    Boolean result = false;

    Tracer::Operation op;
    op.Name = "{{ class }}::HasProperty()";
    op.ObjectID = objectID;
    op.ClientPID = clientPID;
    op.PropertyAddress = address;

    GetContext()->Tracer->OperationBegin(op);

    if (!address) {
        GetContext()->Tracer->Message("address is null");
        result = false;
        goto end;
    }

    if (objectID == GetID()) {
        switch (address->mSelector) {
        {% for prop_name, prop in properties.items() %}
        {% if prop.is_gettable %}
        case {{ prop.id }}:
            {
                {% if prop.allowed_scopes %}
                switch (address->mScope) {
                {% for scope in prop.allowed_scopes %}
                case {{ scope }}:
                {% endfor %}
                    {
                        GetContext()->Tracer->Message("returning HasProperty=true");
                        result = true;
                    }
                    break;
                default:
                    {
                        GetContext()->Tracer->Message(
                            "returning HasProperty=false (disallowed scope)");
                        result = false;
                    }
                    break;
                }
                {% else %}
                GetContext()->Tracer->Message("returning HasProperty=true");
                result = true;
                {% endif %}
            }
            goto end;
        {% endif %}
        {% endfor %}
        }
    }

    {% if base_class %}
    result = {{ base_class }}::HasProperty(objectID, clientPID, address);
    {% else %}
    result = HasPropertyFallback(objectID, clientPID, address);
    {% endif %}

end:
    GetContext()->Tracer->OperationEnd(op, kAudioHardwareNoError);
    return result;
}

OSStatus {{ class }}::IsPropertySettable(AudioObjectID objectID,
    pid_t clientPID,
    const AudioObjectPropertyAddress* address,
    Boolean* outIsSettable) const
{
    OSStatus status = kAudioHardwareNoError;

    Tracer::Operation op;
    op.Name = "{{ class }}::IsPropertySettable()";
    op.ObjectID = objectID;
    op.ClientPID = clientPID;
    op.PropertyAddress = address;

    GetContext()->Tracer->OperationBegin(op);

    if (!address) {
        GetContext()->Tracer->Message("address is null");
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (objectID == GetID()) {
        switch (address->mSelector) {
        {% for prop_name, prop in properties.items() %}
        {% if prop.is_gettable %}
        {% set settable = 'true' if prop.get('is_settable', False) else 'false' %}
        case {{ prop.id }}:
            {
                if (outIsSettable) {
                    GetContext()->Tracer->Message("returning IsSettable={{ settable }}");
                    *outIsSettable = {{ settable }};
                } else {
                    GetContext()->Tracer->Message("output buffer is null");
                }
            }
            goto end;
        {% endif %}
        {% endfor %}
        }
    }

    {% if base_class %}
    status = {{ base_class }}::IsPropertySettable(objectID, clientPID, address, outIsSettable);
    {% else %}
    status = IsPropertySettableFallback(objectID, clientPID, address, outIsSettable);
    {% endif %}

end:
    GetContext()->Tracer->OperationEnd(op, status);
    return status;
}

OSStatus {{ class }}::GetPropertyDataSize(AudioObjectID objectID,
    pid_t clientPID,
    const AudioObjectPropertyAddress* address,
    UInt32 qualifierDataSize,
    const void* qualifierData,
    UInt32* outDataSize) const
{
    OSStatus status = kAudioHardwareNoError;

    Tracer::Operation op;
    op.Name = "{{ class }}::GetPropertyDataSize()";
    op.ObjectID = objectID;
    op.ClientPID = clientPID;
    op.PropertyAddress = address;
    op.QualifierDataSize = qualifierDataSize;
    op.QualifierData = qualifierData;
    op.OutDataSize = outDataSize;

    GetContext()->Tracer->OperationBegin(op);

    if (!address) {
        GetContext()->Tracer->Message("address is null");
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (objectID == GetID()) {
        switch (address->mSelector) {
        {% for prop_name, prop in properties.items() %}
        {% if prop.is_gettable %}
        {% set prop_args = 'address->mScope' if prop.get('scoped_getter', False) else '' %}
        case {{ prop.id }}:
            {
                {% if prop.is_array %}
                if (outDataSize) {
                    const auto values = Get{{ prop_name }}({{ prop_args }});
                    *outDataSize = UInt32(values.size()
                        * sizeof({{ prop.type }}));
                    GetContext()->Tracer->Message("returning PropertySize=%u",
                        unsigned(*outDataSize));
                } else {
                    GetContext()->Tracer->Message("output buffer is null");
                }
                {% else %}
                if (outDataSize) {
                    *outDataSize = sizeof({{ prop.type }});
                    GetContext()->Tracer->Message("returning PropertySize=%u",
                        unsigned(*outDataSize));
                } else {
                    GetContext()->Tracer->Message("output buffer is null");
                }
                {% endif %}
            }
            goto end;
        {% endif %}
        {% endfor %}
        }
    }

    {% if base_class %}
    status = {{ base_class }}::GetPropertyDataSize(
        objectID, clientPID, address, qualifierDataSize, qualifierData, outDataSize);
    {% else %}
    status = GetPropertyDataSizeFallback(
        objectID, clientPID, address, qualifierDataSize, qualifierData, outDataSize);
    {% endif %}

end:
    GetContext()->Tracer->OperationEnd(op, status);
    return status;
}

OSStatus {{ class }}::GetPropertyData(AudioObjectID objectID,
    pid_t clientPID,
    const AudioObjectPropertyAddress* address,
    UInt32 qualifierDataSize,
    const void* qualifierData,
    UInt32 inDataSize,
    UInt32* outDataSize,
    void* outData) const
{
    OSStatus status = kAudioHardwareNoError;

    Tracer::Operation op;
    op.Name = "{{ class }}::GetPropertyData()";
    op.ObjectID = objectID;
    op.ClientPID = clientPID;
    op.PropertyAddress = address;
    op.QualifierDataSize = qualifierDataSize;
    op.QualifierData = qualifierData;
    op.InDataSize = inDataSize;
    op.OutDataSize = outDataSize;
    op.OutData = outData;

    GetContext()->Tracer->OperationBegin(op);

    if (!address) {
        GetContext()->Tracer->Message("address is null");
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (objectID == GetID()) {
        switch (address->mSelector) {
        {% for prop_name, prop in properties.items() %}
        {% if prop.is_gettable %}
        {% set prop_args = 'address->mScope' if prop.get('scoped_getter', False) else '' %}
        case {{ prop.id }}:
            {
                {% if prop.is_array %}
                const auto values = Get{{ prop_name }}({{ prop_args }});
                {% if prop.is_truncatable %}
                const size_t valuesCount = std::min(
                    inDataSize / sizeof({{ prop.type }}),
                    values.size());
                {% else %}
                const size_t valuesCount = values.size();
                if (inDataSize < valuesCount * sizeof({{ prop.type }})) {
                    GetContext()->Tracer->Message("not enough space: need %u, avail %u",
                        unsigned(valuesCount * sizeof({{ prop.type }})),
                        unsigned(inDataSize));
                    status = kAudioHardwareBadPropertySizeError;
                    goto end;
                }
                {% endif %}
                if (outDataSize) {
                    *outDataSize = UInt32(valuesCount
                        * sizeof({{ prop.type }}));
                } else {
                    GetContext()->Tracer->Message("size buffer is null");
                }
                if (outData) {
                    for (size_t i = 0; i < valuesCount; i++) {
                        Convert::ToFoundation(
                            values[i],
                            static_cast<{{ prop.type }}*>(outData)[i]);
                    }
                    GetContext()->Tracer->Message(
                        "returning {{ prop_name }}=%s (%u/%u)",
                        Convert::ToString(values).c_str(),
                        unsigned(valuesCount),
                        unsigned(values.size()));
                } else {
                    GetContext()->Tracer->Message("data buffer is null");
                }
                {% else %}
                if (inDataSize < sizeof({{ prop.type }})) {
                    GetContext()->Tracer->Message("not enough space: need %u, avail %u",
                        unsigned(sizeof({{ prop.type }})),
                        unsigned(inDataSize));
                    status = kAudioHardwareBadPropertySizeError;
                    goto end;
                }
                if (outDataSize) {
                    *outDataSize = sizeof({{ prop.type }});
                } else {
                    GetContext()->Tracer->Message("size buffer is null");
                }
                if (outData) {
                    {% if prop.is_converter %}
                    std::remove_cv_t<std::remove_reference_t<method_return_type_t<
                        decltype(&{{ class }}::Convert{{ prop_name }})>>> inputValue;
                    Convert::FromFoundation(
                        *static_cast<const {{ prop.type }}*>(outData),
                        inputValue);
                    GetContext()->Tracer->Message(
                        "obtained input {{ prop_name }}=%s",
                        Convert::ToString(inputValue).c_str());
                    const auto value = Convert{{ prop_name }}(inputValue);
                    {% elif prop.is_qualified %}
                    if (qualifierDataSize != sizeof({{ prop.qualifier_type }})) {
                        GetContext()->Tracer->Message(
                            "invalid qualifier size: should be %u, got %u",
                            unsigned(sizeof({{ prop.qualifier_type }})),
                            unsigned(qualifierDataSize));
                        status = kAudioHardwareBadPropertySizeError;
                        goto end;
                    }
                    std::remove_cv_t<std::remove_reference_t<method_argument_type_t<
                        decltype(&{{ class }}::Get{{ prop_name }}), 0>>> qualifierValue;
                    Convert::FromFoundation(
                        *static_cast<const {{ prop.qualifier_type }}*>(qualifierData),
                        qualifierValue);
                    GetContext()->Tracer->Message(
                        "obtained qualifier {{ prop_name }}=%s",
                        Convert::ToString(qualifierValue).c_str());
                    const auto value = Get{{ prop_name }}(qualifierValue);
                    {% else %}
                    const auto value = Get{{ prop_name }}({{ prop_args }});
                    {% endif %}
                    Convert::ToFoundation(
                        value,
                        *static_cast<{{ prop.type }}*>(outData));
                    GetContext()->Tracer->Message(
                        "returning {{ prop_name }}=%s",
                        Convert::ToString(value).c_str());
                } else {
                    GetContext()->Tracer->Message("data buffer is null");
                }
                {% endif %}
            }
            goto end;
        {% endif %}
        {% endfor %}
        }
    }

    {% if base_class %}
    status = {{ base_class }}::GetPropertyData(
        objectID, clientPID, address, qualifierDataSize, qualifierData,
        inDataSize, outDataSize, outData);
    {% else %}
    status = GetPropertyDataFallback(
        objectID, clientPID, address, qualifierDataSize, qualifierData,
        inDataSize, outDataSize, outData);
    {% endif %}

end:
    GetContext()->Tracer->OperationEnd(op, status);
    return status;
}

OSStatus {{ class }}::SetPropertyData(AudioObjectID objectID,
    pid_t clientPID,
    const AudioObjectPropertyAddress* address,
    UInt32 qualifierDataSize,
    const void* qualifierData,
    UInt32 inDataSize,
    const void* inData)
{
    OSStatus status = kAudioHardwareNoError;

    Tracer::Operation op;
    op.Name = "{{ class }}::SetPropertyData()";
    op.ObjectID = objectID;
    op.ClientPID = clientPID;
    op.PropertyAddress = address;
    op.QualifierDataSize = qualifierDataSize;
    op.QualifierData = qualifierData;
    op.InDataSize = inDataSize;
    op.InData = inData;

    GetContext()->Tracer->OperationBegin(op);

    if (!address) {
        GetContext()->Tracer->Message("address is null");
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if (objectID == GetID()) {
        switch (address->mSelector) {
        {% for prop_name, prop in properties.items() %}
        {% if prop.is_settable %}
        {% set prop_suffix = 'Async' if prop.get('is_async', False) else '' %}
        case {{ prop.id }}:
            {
                {% if prop.is_array %}
                if (inDataSize % sizeof({{ prop.type }}) != 0) {
                    GetContext()->Tracer->Message(
                        "invalid size: should be multiple of %u, got %u",
                        unsigned(sizeof({{ prop.type }})), unsigned(inDataSize));
                    status = kAudioHardwareBadPropertySizeError;
                    goto end;
                }
                if (inDataSize != 0 && !inData) {
                    GetContext()->Tracer->Message("input buffer is null");
                    status = kAudioHardwareIllegalOperationError;
                    goto end;
                }
                const size_t valuesCount = inDataSize / sizeof({{ prop.type }});
                std::vector<std::remove_cv_t<std::remove_reference_t<
                    decltype(Get{{ prop_name }}())>> values(valuesCount);
                for (size_t i = 0; i < valuesCount; i++) {
                    Convert::FromFoundation(
                        static_cast<const {{ prop.type }}*>(inData)[i],
                        values[i]);
                }
                GetContext()->Tracer->Message(
                    "setting {{ prop_name }}=%s",
                    Convert::ToString(values).c_str());
                status = Set{{ prop_name }}{{ prop_suffix }}(std::move(values));
                if (status != kAudioHardwareNoError) {
                    GetContext()->Tracer->Message("setter failed");
                    goto end;
                }
                {% else %}
                if (inDataSize != sizeof({{ prop.type }})) {
                    GetContext()->Tracer->Message("invalid size: should be %u, got %u",
                        unsigned(sizeof({{ prop.type }})), unsigned(inDataSize));
                    status = kAudioHardwareBadPropertySizeError;
                    goto end;
                }
                if (!inData) {
                    GetContext()->Tracer->Message("input buffer is null");
                    status = kAudioHardwareIllegalOperationError;
                    goto end;
                }
                std::remove_cv_t<std::remove_reference_t<
                    decltype(Get{{ prop_name }}())>> value;
                Convert::FromFoundation(
                    *static_cast<const {{ prop.type }}*>(inData),
                    value);
                GetContext()->Tracer->Message(
                    "setting {{ prop_name }}=%s",
                    Convert::ToString(value).c_str());
                status = Set{{ prop_name }}{{ prop_suffix }}(std::move(value));
                if (status != kAudioHardwareNoError) {
                    GetContext()->Tracer->Message("setter failed");
                    goto end;
                }
                {% endif %}
            }
            goto end;
        {% endif %}
        {% endfor %}
        }
    }

    {% if base_class %}
    status = {{ base_class }}::SetPropertyData(
        objectID, clientPID, address, qualifierDataSize, qualifierData, inDataSize, inData);
    {% else %}
    status = SetPropertyDataFallback(
        objectID, clientPID, address, qualifierDataSize, qualifierData, inDataSize, inData);
    {% endif %}

end:
    GetContext()->Tracer->OperationEnd(op, status);
    return status;
}

} // namespace aspl
'''.strip())

text = template.render(
    **builtins.__dict__,
    **klass,
    generator_script=os.path.basename(__file__),
    generator_input=os.path.basename(args.i),
    )

with open(args.o, 'w') as out:
    print(text, file=out)
