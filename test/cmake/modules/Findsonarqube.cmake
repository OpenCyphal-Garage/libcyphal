#
# Find sonar-scanner.
#

find_program(SONARQUBE sonar-scanner)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(sonar-scanner
    REQUIRED_VARS SONARQUBE
)
