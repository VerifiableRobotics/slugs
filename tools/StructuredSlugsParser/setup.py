#!/usr/bin/env python

from setuptools import setup

long_description = (
    'Slugs is a stand-alone reactive synthesis tool for '
    'generalized reactivity(1) synthesis.'
)

setup(
    name='slugs',
    version='0.1',
    license='BSD',
    description='Solve generalized reactivity(1) games.',
    long_description=long_description,
    author='Ruediger Ehlers, Vasumathi Raman, and Cameron Finucane',
    #author_email='',
    url = 'https://github.com/LTLMoP/slugs',
    keywords = ['linear temporal logic', 'reactive synthesis', 'games'],
    packages=['slugs'],
)
