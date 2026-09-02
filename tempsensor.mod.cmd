savedcmd_tempsensor.mod := printf '%s\n'   tempsensor.o | awk '!x[$$0]++ { print("./"$$0) }' > tempsensor.mod
