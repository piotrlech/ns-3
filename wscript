# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('pio', ['core','internet','network'])
    module.source = [
        'model/pior.cc',
        'model/aqm.cc',
        'helper/pior-helper.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'pio'
    headers.source = [
        'model/pior.h',
        'model/aqm.h',
        'helper/pior-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

