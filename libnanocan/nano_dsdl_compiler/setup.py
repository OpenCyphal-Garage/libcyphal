#!/usr/bin/env python

from distutils.core import setup

args = dict(
    name='libnanocan_dsdl_compiler',
    version='0.1',
    description='UAVCAN DSDL compiler for libnanocan',
    packages=['libnanocan_dsdl_compiler'],
    package_data={'libnanocan_dsdl_compiler': ['data_type_template.tmpl']},
    scripts=['libnanocan_dsdlc'],
    requires=['pyuavcan'],
    author='Pavel Kirienko',
    author_email='pavel.kirienko@gmail.com',
    url='http://uavcan.org',
    license='MIT'
)

setup(**args)
