#
# UAVCAN DSDL compiler for libuavcan
#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

'''
This module implements the core functionality of the UAVCAN DSDL compiler for libuavcan.
Supported Python versions: 3.2+.
It accepts a list of root namespaces and produces the set of C++ header files for libuavcan.
It is based on the pydsdl.
'''

import sys, os, logging, errno, re
from pydsdl import parse_namespace
from pydsdl.parse_error import InvalidDefinitionError
from pydsdl.parse_error import InternalError
from pydsdl.data_type import CompoundType, PrimitiveType, ArrayType, StaticArrayType, \
                             DynamicArrayType, FloatType, VoidType, BooleanType, \
                             SignedIntegerType, UnsignedIntegerType, ServiceType, \
                             UnionType
from .pyratemp import Template

OUTPUT_FILE_EXTENSION = 'hpp'
OUTPUT_FILE_PERMISSIONS = 0o444  # Read only for all

__all__ = ['run', 'logger', 'DsdlCompilerException']

class DsdlCompilerException(Exception):
    pass

logger = logging.getLogger(__name__)

def run(source_dirs, include_dirs, output_dir, template_path, dry_run=False):
    '''
    This function takes a list of root namespace directories (containing DSDL definition files to parse), a
    possibly empty list of search directories (containing DSDL definition files that can be referenced from the types
    that are going to be parsed), and the output directory path (possibly nonexistent) where the generated C++
    header files will be stored.

    Note that this module features lazy write, i.e. if an output file does already exist and its content is not going
    to change, it will not be overwritten. This feature allows to avoid unnecessary recompilation of dependent object
    files.

    Args:
        source_dirs         List of root namespace directories to parse.
        include_dirs        List of root namespace directories with referenced types (possibly empty). This list is
                            automatically extended with source_dirs.
        output_dir          Output directory path. Will be created if doesn't exist.
        template_path       Path to the pyratemp template used to generate C++ headers.
        dry_run             If True no output files are generated.
    '''
    assert isinstance(source_dirs, list)
    assert isinstance(include_dirs, list)
    output_dir = str(output_dir)

    types = []
    for source_dir in source_dirs:
        logger.info("source_dir:{}".format(source_dir))
        types_for_source_dir = run_parser(source_dir, include_dirs + source_dirs)
        types += types_for_source_dir

    if len(types) == 0:
        die('No type definitions were found')

    logger.info('%d types total', len(types))
    run_generator(types, output_dir, template_path, dry_run)

# -----------------

def pretty_filename(filename):
    try:
        a = os.path.abspath(filename)
        r = os.path.relpath(filename)
        return a if '..' in r else r
    except ValueError:
        return filename

def type_output_filename(path, t):
    assert isinstance(t, CompoundType)
    return os.path.join(path, *t.name_components) + '.' + OUTPUT_FILE_EXTENSION

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

def run_parser(source_dir, search_dirs):
    try:
        return parse_namespace(source_dir, search_dirs)
    except InvalidDefinitionError as ex:
        logger.error('Invalid DSDL definition encountered.', exc_info=True)
        die(ex)
    except InternalError as ex:
        logger.error('Internal error:', exc_info=True)
        die(ex)

def run_generator(types, dest_dir, template_path, dry_run=False):
    try:
        dest_dir = os.path.abspath(dest_dir)  # Removing '..'
        if not dry_run:
            template_expander = make_template_expander(template_path)
            makedirs(dest_dir)
        else:
            template_expander = None
        for t in types:
            filename = type_output_filename(dest_dir, t)
            if not dry_run:
                logger.info('Generating type %s', t.full_name)
                text = generate_one_type(template_expander, t)
                write_generated_data(filename, text)
            else:
                logger.info('Would have generated type %s as file :', t.full_name)
                sys.stdout.write(filename)
                sys.stdout.write(';')
    except Exception as ex:
        logger.info('Generator failure', exc_info=True)
        die(ex)

def write_generated_data(filename, data):
    dirname = os.path.dirname(filename)
    makedirs(dirname)

    if os.path.exists(filename):
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

def type_to_cpp_type(t):
    if isinstance(t, PrimitiveType):
        cast_mode = {
            PrimitiveType.CastMode.SATURATED: '::uavcan::CastModeSaturate',
            PrimitiveType.CastMode.TRUNCATED: '::uavcan::CastModeTruncate',
        }[t.cast_mode]
        if isinstance(t, FloatType):
            return '::uavcan::FloatSpec< %d, %s >' % (t.bit_length, cast_mode)
        else:
            if isinstance(t, BooleanType) or isinstance(t, UnsignedIntegerType):
                signedness = '::uavcan::SignednessUnsigned'
            elif isinstance(t, SignedIntegerType):
                signedness = '::uavcan::SignednessSigned'
            else:
                raise DsdlCompilerException('Unknown primative type: %r' % t)
            return '::uavcan::IntegerSpec< %d, %s, %s >' % (t.bit_length, signedness, cast_mode)
    elif isinstance(t, ArrayType):
        value_type = type_to_cpp_type(t.element_type)
        if isinstance(t, StaticArrayType):
            mode = "::uavcan::ArrayModeStatic"
            size = t.size
        elif isinstance(t, DynamicArrayType):
            mode = "::uavcan::ArrayModeDynamic"
            size = t.max_size
        else:
            raise DsdlCompilerException('Unknown array type: %r' % t)
        return '::uavcan::Array< %s, %s, %d >' % (value_type, mode, size)
    elif isinstance(t, CompoundType):
        return '::' + t.full_name.replace('.', '::')
    elif isinstance(t, VoidType):
        return '::uavcan::IntegerSpec< %d, ::uavcan::SignednessUnsigned, ::uavcan::CastModeSaturate >' % t.bit_length
    else:
        raise DsdlCompilerException('Unknown type: %r' % t)

class PortingShim(dict):

    def __getattr__(self, attr):
        return self[attr]

    def __setattr__(self, attr, value):
        self[attr] = value

    def get_max_bitlen(self):
        return 0

    def get_max_bitlen_request(self):
        return self.get_max_bitlen()

    def get_max_bitlen_response(self):
        return self.get_max_bitlen()

    def get_dsdl_signature(self):
        return 0

    def get_dsdl_signature_source_definition(self):
        return ""

def generate_one_type(template_expander, in_datatype):
    template_params = PortingShim()
    template_params["source_file"] = in_datatype.source_file_path
    template_params["source_text"] = "TODO"
    template_params["short_name"] = in_datatype.full_name.split('.')[-1]
    template_params["cpp_type_name"] = in_datatype.short_name + '_'
    template_params["cpp_full_type_name"] = '::' + in_datatype.full_name.replace('.', '::')
    template_params["include_guard"] = in_datatype.full_name.replace('.', '_').upper() + '_HPP_INCLUDED'
    template_params["is_service"] = isinstance(in_datatype, ServiceType)
    template_params["full_name"] = in_datatype.full_name

    # Dependencies (no duplicates)
    def fields_includes(fields):
        def detect_include(t):
            if isinstance(t, CompoundType):
                return type_output_filename("", t)
            if isinstance(t, ArrayType):
                return detect_include(t.element_type)
        return list(sorted(set(filter(None, [detect_include(x.data_type) for x in fields]))))

    template_params["cpp_includes"] = fields_includes(in_datatype.fields)

    template_params["cpp_namespace_components"] = in_datatype.full_name.split('.')[:-1]
    template_params["has_default_dtid"] = in_datatype.has_fixed_port_id
    template_params["default_dtid"] = in_datatype.fixed_port_id

    # Attribute types
    def inject_cpp_types(key, attributes, out_template_params):
        void_index = 0
        out_attributes = []
        out_template_params[key] = out_attributes
        for i in range(0, len(attributes)):
            a = attributes[i]
            out_a = {}
            out_attributes += out_a
            out_a["cpp_type"] = type_to_cpp_type(a.data_type)
            out_a["void"] = isinstance(a, VoidType)
            if out_a["void"]:
                assert not a["name"]
                out_a["name"] = '_void_%d' % void_index
                void_index += 1
            else:
                out_a["name"] = a.name

    inject_cpp_types("fields", in_datatype.fields, template_params)
    inject_cpp_types("constants", in_datatype.constants, template_params)
    template_params["request_fields"] = (i for i in template_params.fields if i.name == "request")
    template_params["response_fields"] = (i for i in template_params.fields if i.name == "response")
    template_params["all_attributes"] = template_params.fields + template_params.constants

    if not isinstance(in_datatype, ServiceType):
        template_params["union"] = isinstance(in_datatype, UnionType) and len(template_params.fields)
    else:
        template_params["request_union"] = isinstance(in_datatype, UnionType) and len(template_params.request_fields)
        template_params["response_union"] = isinstance(in_datatype, UnionType) and len(template_params.response_fields)

    # Constant properties
    def inject_constant_info(constants, out_template_params):
        out_constants = []
        out_template_params["constants"] = out_constants
        for i in range(0, len(constants)):
            c = constants[i]
            constant_params = {}
            out_constants += constant_params
            if isinstance(c, FloatType):
                float(c.string_value)  # Making sure that this is a valid float literal
                constant_params["cpp_value"] = c.string_value
            else:
                int(c.string_value)  # Making sure that this is a valid integer literal
                constant_params["cpp_value"] = c.string_value
                if isinstance(c, UnsignedIntegerType):
                    constant_params["cpp_value"] += 'U'

    inject_constant_info(template_params.constants, template_params)
    # just clone for now. This is total junk but I want to get this
    # compiling first!
    template_params["request_constants"] = template_params["constants"]
    template_params["response_constants"] = template_params["constants"]

    # Data type kind
    template_params["cpp_kind"] = '::uavcan::DataTypeKindService' if isinstance(in_datatype, ServiceType) else '::uavcan::DataTypeKindMessage'

    # Generation
    text = template_expander(t=template_params)  # t for Type
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
    template_text = re.sub(r'(?m)^(\ *)\%\ *(.+?):{0,1}$', r'\1<!--(\2)-->', template_text)

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
        # This function works like enumerate(), telling you whether the current item is the last one
        def enum_last_value(iterable, start=0):
            it = iter(iterable)
            count = start
            try:
                last = next(it)
            except StopIteration:
                return
            for val in it:
                yield count, False, last
                last = val
                count += 1
            yield count, True, last
        args['enum_last_value'] = enum_last_value
        return template(**args)

    return expand
