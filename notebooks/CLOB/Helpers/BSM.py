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

def BlackScholesPaths(
    S_0: float, 
    r: float, 
    sigma: float, 
    T: float, 
    N: int, 
    n: int,
    rng: np.random.Generator
):
    delta = T / N
    Z = rng.normal(loc=0, scale=1, size=(n, N))
    path = np.empty((n, N + 1), dtype=np.float64)
    path[:, 0] = S_0
    for i in range(N):
        path[:, i + 1] = path[:, i] * np.exp((r - 0.5 * np.power(sigma, 2.0)) * delta + sigma * np.sqrt(delta) * Z[:, i])
    return path
