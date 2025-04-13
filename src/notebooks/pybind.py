import os
import sys

sys.path.insert(0, os.path.abspath("python_modules"))
import Server

assert Server.meaning_of_life() == 42

simulation = Server.BindSimulation.create()
tickers = simulation.get_tickers()

BOND_ID = tickers.index("BOND")
CAD_ID = tickers.index("CAD")
print(f"Bond: {BOND_ID}, CAD: {CAD_ID}")