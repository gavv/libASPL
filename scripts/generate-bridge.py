#! /usr/bin/env python3
import argparse
import builtins
import collections
import datetime
import jinja2
import json
import os
import sys

func_fields = [
    'object_type',
    'return',
    'args',
]

parser = argparse.ArgumentParser()
parser.add_argument('-i', type=str, help='input json file', required=True)
parser.add_argument('-o', type=str, help='output c++ file', required=True)

args = parser.parse_args()

with open(args.i) as json_file:
    funcs = json.load(json_file, object_pairs_hook=collections.OrderedDict)

for func in funcs.values():
    for field in func.keys():
        if not field in func_fields:
            raise RuntimeError("unknown field %s" % field)

env = jinja2.Environment(
    trim_blocks=True,
    lstrip_blocks=True,
    undefined = jinja2.StrictUndefined)

template = env.from_string('''
// THIS FILE IS AUTO-GENERATED. DO NOT EDIT!

// Generator: {{ generator_script }}
// Source: {{ generator_input }}
// Timestamp: {{ timestamp }}

// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Driver.hpp>
#include <aspl/Tracer.hpp>

#include "Bridge.hpp"

namespace aspl {

{% for func_name, func in funcs.items() %}
{% for arg_num, arg_name in enumerate(func.args) %}
{% set arg_type = func.args[arg_name] %}
{% if arg_num == 0 %}
{{ func.return }} Bridge::{{ func_name }}({{ arg_type }} {{ arg_name }},
{% elif arg_num != len(func.args) - 1 %}
    {{ arg_type }} {{ arg_name }},
{% else %}
    {{ arg_type }} {{ arg_name }})
{% endif %}
{% endfor %}
{
    const auto driver = Driver::GetDriver(driverRef);
{% if func.return == 'OSStatus' %}
    if (!driver) {
        return kAudioHardwareUnspecifiedError;
    }
{% else %}
    if (!driver) {
        return {};
    }
{% endif %}

    {{ func.return }} result = {};

    const auto object = driver->GetContext()->Dispatcher->FindObject(objectID);
    if (!object) {
        driver->GetContext()->Tracer->Message(
            "Bridge::{{ func_name }}() objectID=%u object not registered",
             unsigned(objectID));
{% if func.return == 'OSStatus' %}
        result = kAudioHardwareBadObjectError;
{% endif %}
        goto end;
    }

{% for arg_num, arg_name in enumerate(func.args) %}
{% if arg_num == 0 %}
{% elif arg_num == 1 %}
    result = std::static_pointer_cast<{{ func.object_type }}>(object)->{{ func_name }}(
        {{ arg_name }},
{% elif arg_num != len(func.args) - 1 %}
        {{ arg_name }},
{% else %}
        {{ arg_name }});
{% endif %}
{% endfor %}

end:
    return result;
}

{% endfor %}
} // namespace aspl
'''.strip())

text = template.render(
    **builtins.__dict__,
    funcs=funcs,
    generator_script=os.path.basename(__file__),
    generator_input=os.path.basename(args.i),
    timestamp=datetime.datetime.now(datetime.UTC).strftime("%a %b %d %H:%M:%S %Y UTC"),
    )

with open(args.o, 'w') as out:
    print(text, file=out)
