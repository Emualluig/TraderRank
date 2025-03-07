import numpy as np
import numpy.typing as npt

def BlackScholesFinalPrices(
    S_0: float, 
    r: float, 
    sigma: float, 
    T: float, 
    n: int,
    rng: np.random.Generator
) -> npt.NDArray[np.float64]:
    Z = rng.normal(loc=0, scale=1, size=(n))
    return S_0 * np.exp((r - 0.5 * sigma**2) * T + sigma * np.sqrt(T) * Z)
