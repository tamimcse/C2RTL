export DESIGN_NAME = top
export PLATFORM    = nangate45

export VERILOG_FILES = ./designs/src/cutsplit/top.v
export SDC_FILE      = ./designs/src/cutsplit/top.sdc

# These values must be multiples of placement site
# x=0.19 y=1.4
export DIE_AREA    = 0 0 850.13 850.8
export CORE_AREA   = 10.07 11.2 840.25 841

export CLOCK_PERIOD = 1.000
