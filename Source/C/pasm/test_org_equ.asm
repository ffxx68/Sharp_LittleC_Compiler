; Test .ORG e .EQU per pasm migrato
.ORG 40000
.EQU regB 3
START:
    LIA regB
    JRM START

