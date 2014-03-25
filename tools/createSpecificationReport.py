#!/usr/bin/python
#

import random
import os
import sys
import subprocess, md5, time, cgi

# =====================================================
# Option configuration
# =====================================================
slugsExecutableAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("createSpecificationReport.py")]+"../src/slugs"
slugsReturnFile = "/tmp/check_"+str(os.getpid())+".slugsreturn"
slugsErrorFile = "/tmp/check_"+str(os.getpid())+".slugserr"

# =====================================================
# Main function
# =====================================================
def createSpecificationReport(slugsFile):

    # =====================================================
    # Make HTML Barebone
    # =====================================================
    print "<!DOCTYPE html>\n<html>\n<head><title>Specification report for input file "+cgi.escape(slugsFile)+"</title>"
    print "<style media=\"all\" type=\"text/css\">"
    print "body{font-family:\"Verdana\", Verdana, serif;}"
    print "h1{ text-align: center; }"
    print "summary {"
    print "     font-size: 150%;"
    print "}"
    print " .details {"
    print "      margin-left: 30px;"
    print "}"
    print "</style>"
    print "</head>"
    print "<body>"

    # =====================================================
    # Make a page header
    # =====================================================
    print "<H1>Slugs Specification report for file <I>"+cgi.escape(slugsFile)+"</I></H1><HR/>"

    # =====================================================
    # Specification
    # =====================================================
    print "<details>"
    print "<summary>1. Specification</summary>"
    print "<pre class=\"details\">"
    with open(slugsFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"


    # =====================================================
    # Close up HTML File
    # =====================================================

    print "</body></html>"

    command = slugsExecutableAndBasicOptions + " --onlyRealizability "+slugsFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")



# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)>1:
        slugsFile = sys.argv[1]
    else:
        print >>sys.stderr,"Error: Expected non-incremental slugs file name as input."
        sys.exit(1)

    createSpecificationReport(slugsFile)

