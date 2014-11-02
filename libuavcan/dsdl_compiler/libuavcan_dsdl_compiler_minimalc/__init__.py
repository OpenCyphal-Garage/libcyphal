#
# UAVCAN DSDL compiler for libuavcan
#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
#

'''
This module implements the core functionality of the UAVCAN DSDL compiler for libuavcan.
Supported Python versions: 3.2+, 2.7.
It accepts a list of root namespaces and produces the set of C marshalling functions for libuavcan.
It is based on the DSDL parsing package from pyuavcan.
'''

from __future__ import division, absolute_import, print_function, unicode_literals
import sys, os, logging, errno, re
from .pyratemp import Template
from pyuavcan import dsdl

# Python 2.7 compatibility
try:
    str = unicode
except NameError:
    pass

OUTPUT_FILE_EXTENSION = 'c'
OUTPUT_FILE_PERMISSIONS = 0o444  # Read only for all
TEMPLATE_FILENAME = os.path.join(os.path.dirname(__file__), 'data_type_template.tmpl')

__all__ = ['run', 'logger', 'DsdlCompilerException']

class DsdlCompilerException(Exception):
    pass

logger = logging.getLogger(__name__)

def run(source_dirs, include_dirs, output_dir):
    '''
    This function takes a list of root namespace directories (containing DSDL definition files to parse), a
    possibly empty list of search directories (containing DSDL definition files that can be referenced from the types
    that are going to be parsed), and the output directory path (possibly nonexistent) where the generated C files will be stored.
    
    Note that this module features lazy write, i.e. if an output file does already exist and its content is not going
    to change, it will not be overwritten. This feature allows to avoid unnecessary recompilation of dependent object
    files.
    
    Args:
        source_dirs    List of root namespace directories to parse.
        include_dirs   List of root namespace directories with referenced types (possibly empty). This list is
                       automaitcally extended with source_dirs.
        output_dir     Output directory path. Will be created if doesn't exist.
    '''
    assert isinstance(source_dirs, list)
    assert isinstance(include_dirs, list)
    output_dir = str(output_dir)

    types = run_parser(source_dirs, include_dirs + source_dirs)
    if not types:
        die('No type definitions were found')

    logger.info('%d types total', len(types))
    run_generator(types, output_dir)

# -----------------

def pretty_filename(filename):
    try:
        a = os.path.abspath(filename)
        r = os.path.relpath(filename)
        return a if '..' in r else r
    except ValueError:
        return filename

def type_output_filename(t):
    assert t.category == t.CATEGORY_COMPOUND
    return t.full_name.replace('.', os.path.sep) + '.' + OUTPUT_FILE_EXTENSION

def makedirs(path):
    try:
        try:
            os.makedirs(path, exist_ok=True)  # May throw "File exists" when executed as root, which is wrong
        except TypeError:
            os.makedirs(path)  # Python 2.7 compatibility
    except OSError as ex:
        if ex.errno != errno.EEXIST:  # http://stackoverflow.com/questions/12468022
            raise

def die(text):
    raise DsdlCompilerException(str(text))

def run_parser(source_dirs, search_dirs):
    try:
        types = dsdl.parse_namespaces(source_dirs, search_dirs)
    except dsdl.DsdlException as ex:
        logger.info('Parser failure', exc_info=True)
        die(ex)
    return types

def run_generator(types, dest_dir):
    try:
        template_expander = make_template_expander(TEMPLATE_FILENAME)
        dest_dir = os.path.abspath(dest_dir)  # Removing '..'
        makedirs(dest_dir)
        for t in types:
            logger.info('Generating type %s', t.full_name)
            filename = os.path.join(dest_dir, type_output_filename(t))
            text = generate_one_type(template_expander, t)
            write_generated_data(filename, text)
    except Exception as ex:
        logger.info('Generator failure', exc_info=True)
        die(ex)

def write_generated_data(filename, data):
    dirname = os.path.dirname(filename)
    makedirs(dirname)

    # Lazy update - file will not be rewritten if its content is not going to change
    if os.path.exists(filename):
        with open(filename) as f:
            existing_data = f.read()
        if data == existing_data:
            logger.info('Up to date [%s]', pretty_filename(filename))
            return
        logger.info('Rewriting [%s]', pretty_filename(filename))
        os.remove(filename)
    else:
        logger.info('Creating [%s]', pretty_filename(filename))

    # Full rewrite
    with open(filename, 'w') as f:
        f.write(data)
    try:
        os.chmod(filename, OUTPUT_FILE_PERMISSIONS)
    except (OSError, IOError) as ex:
        logger.warning('Failed to set permissions for %s: %s', pretty_filename(filename), ex)

def type_to_c_type(t, aname, fname):
    if t.category == t.CATEGORY_PRIMITIVE:
        if t.kind == t.KIND_FLOAT:
            return 'float '
        else:
            signedness = {
                t.KIND_BOOLEAN: 'unsigned',
                t.KIND_UNSIGNED_INT: 'unsigned',
                t.KIND_SIGNED_INT: 'signed',
            }[t.kind]
            
            return '%s int' % (signedness)
    elif t.category == t.CATEGORY_ARRAY:
        value_type = type_to_c_type(t.value_type, aname, fname)
        if t.value_type.category != t.CATEGORY_COMPOUND:
            if t.value_type.kind != t.value_type.KIND_FLOAT:
                t.bit_struct = 'typedef struct %s_%s_t {\n    %s %s:%d;\n};\n'%(fname, aname, value_type, aname, t.value_type.get_max_bitlen() )
            else:
                t.bit_struct = 'typedef struct %s_%s_t {\n    %s %s:%d;\n};\n'%(fname, aname, value_type, aname, t.value_type.get_max_bitlen() )
        else:
            t.bit_struct = 'Compound types not supported'
        return 'struct %s_%s_t %s[%d]' % (fname, aname, aname, t.max_size)
    elif t.category == t.CATEGORY_COMPOUND:
        return '::' + t.full_name.replace('.', '::')
    else:
        raise DsdlCompilerException('Unknown type category: %s' % t.category)

def generate_one_type(template_expander, t):
    t.short_name = t.full_name.split('.')[-1]
    t.c_type_name = t.short_name + '_t'
    t.c_function_name = t.full_name.replace('.', '_')
    t.c_full_type_name = t.full_name.replace('.', '_') + '_t'

    # Dependencies (no duplicates)
    def fields_includes(fields):
        def detect_include(t):
            if t.category == t.CATEGORY_COMPOUND:
                return type_output_filename(t)
            if t.category == t.CATEGORY_ARRAY:
                return detect_include(t.value_type)
        return list(sorted(set(filter(None, [detect_include(x.type) for x in fields]))))

    if t.kind == t.KIND_MESSAGE:
        t.c_includes = fields_includes(t.fields)
    else:
        t.c_includes = fields_includes(t.request_fields + t.response_fields)

    t.c_namespace_components = t.full_name.split('.')[:-1]
    t.has_default_dtid = t.default_dtid is not None

    # Attribute types
    def inject_c_types(attributes):
        for a in attributes:
            a.c_type = type_to_c_type(a.type, a.name, t.c_function_name)

    if t.kind == t.KIND_MESSAGE:
        inject_c_types(t.fields)
        inject_c_types(t.constants)
        t.all_attributes = t.fields + t.constants
    else:
        inject_c_types(t.request_fields)
        inject_c_types(t.request_constants)
        inject_c_types(t.response_fields)
        inject_c_types(t.response_constants)
        t.all_attributes = t.request_fields + t.request_constants + t.response_fields + t.response_constants

    # Constant properties
    def inject_constant_info(constants):
        for c in constants:
            if c.type.kind == c.type.KIND_FLOAT:
                float(c.string_value)  # Making sure that this is a valid float literal
                c.c_value = c.string_value
            else:
                int(c.string_value)  # Making sure that this is a valid integer literal
                c.c_value = c.string_value
                if c.type.kind == c.type.KIND_UNSIGNED_INT:
                    c.c_value += 'U'

    if t.kind == t.KIND_MESSAGE:
        inject_constant_info(t.constants)
    else:
        inject_constant_info(t.request_constants)
        inject_constant_info(t.response_constants)

    # Data type kind
    t.c_kind = {
        t.KIND_MESSAGE: '::uavcan::DataTypeKindMessage',
        t.KIND_SERVICE: '::uavcan::DataTypeKindService',
    }[t.kind]

    # Generation
    text = template_expander(t=t)  # t for Type
    text = '\n'.join(x.rstrip() for x in text.splitlines())
    text = text.replace('\n\n\n\n\n', '\n\n').replace('\n\n\n\n', '\n\n').replace('\n\n\n', '\n\n')
    text = text.replace('{\n\n ', '{\n ')
    return text

def make_template_expander(filename):
    '''
    Templating is based on pyratemp (http://www.simple-is-better.org/template/pyratemp.html).
    The pyratemp's syntax is rather verbose and not so human friendly, so we define some
    custom extensions to make it easier to read and write.
    The resulting syntax somewhat resembles Mako (which was used earlier instead of pyratemp):
        Substitution:
            ${expression}
        Line joining through backslash (replaced with a single space):
            ${foo(bar(very_long_arument=42, \
                      second_line=72))}
        Blocks:
            % for a in range(10):
                % if a == 5:
                    ${foo()}
                % endif
            % endfor
    The extended syntax is converted into pyratemp's through regexp substitution.
    '''
    with open(filename) as f:
        template_text = f.read()

    # Backslash-newline elimination
    template_text = re.sub(r'\\\r{0,1}\n\ *', r' ', template_text)

    # Substitution syntax transformation: ${foo} ==> $!foo!$
    template_text = re.sub(r'([^\$]{0,1})\$\{([^\}]+)\}', r'\1$!\2!$', template_text)

    # Flow control expression transformation: % foo: ==> <!--(foo)-->
    template_text = re.sub(r'(?m)^(\ *)\%\ *([^\:]+?):{0,1}$', r'\1<!--(\2)-->', template_text)

    # Block termination transformation: <!--(endfoo)--> ==> <!--(end)-->
    template_text = re.sub(r'\<\!--\(end[a-z]+\)--\>', r'<!--(end)-->', template_text)

    # Pyratemp workaround.
    # The problem is that if there's no empty line after a macro declaration, first line will be doubly indented.
    # Workaround:
    #  1. Remove trailing comments
    #  2. Add a newline after each macro declaration
    template_text = re.sub(r'\ *\#\!.*', '', template_text)
    template_text = re.sub(r'(\<\!--\(macro\ [a-zA-Z0-9_]+\)--\>.*?)', r'\1\n', template_text)

    # Preprocessed text output for debugging
#   with open(filename + '.d', 'w') as f:
#       f.write(template_text)

    template = Template(template_text)

    def expand(**args):
        # This function adds one indentation level (4 spaces); it will be used from the template
        args['indent'] = lambda text, idnt = '    ': idnt + text.replace('\n', '\n' + idnt)
        return template(**args)

    return expand
