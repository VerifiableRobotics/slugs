# -*- coding: utf-8 -*-

from setuptools import setup

long_description = (
    'Slugs is a stand-alone reactive synthesis tool for '
    'generalized reactivity(1) synthesis.'
)

setup(
    name='slugs',
    version='0.1',
    package_dir={'slugs': 'slugs'},
    license='BSD',
    description='Solve generalized reactivity(1) games.',
    long_description=long_description,
    author='Ruediger Ehlers, Vasumathi Raman, and Cameron Finucane',
    #author_email='',
    url = 'https://github.com/LTLMoP/slugs',
    install_requires=['networkx'],
    keywords = ['linear temporal logic', 'reactive synthesis', 'games'],
)
