SOURCE_FILE += simulation.c
SIMULATION_EXECUTABLE = smaug_simulation

all:
  $(CC) -o $(SIMULATION_EXECUTABLE) $(SOURCE_FILE)
