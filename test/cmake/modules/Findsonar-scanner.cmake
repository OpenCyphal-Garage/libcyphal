#
# Find sonar-scanner.
#

find_program(SONAR_SCANNER sonar-scanner)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(sonar-scanner
    REQUIRED_VARS SONAR_SCANNER
)
