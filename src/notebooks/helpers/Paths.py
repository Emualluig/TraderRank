import numpy as np
import numpy.typing as npt

def BSMPathExtended(
    S_0: float, 
    r: float, 
    sigma: float, 
    T: float, 
    N: int, 
    rng: np.random.Generator,
    extension_steps: int
):
    delta = T / N
    Z = rng.normal(loc=0, scale=1, size=(N + extension_steps))
    path = np.empty((N + extension_steps + 1), dtype=np.float64)
    path[0] = S_0
    for i in range(N + extension_steps):
        path[i + 1] = path[i] * np.exp((r - 0.5 * np.power(sigma, 2.0)) * delta + sigma * np.sqrt(delta) * Z[i])
    return path

def MeanRevertingPathFollower(
    underlying_path: npt.NDArray[np.floating],
    reversion_strength: float,
    volatility: float,
    T: float, 
    N: int, 
    rng: np.random.Generator,
    forward_shift: int
):
    dt = T / N
    Z = rng.normal(loc=0, scale=1, size=(N))
    path = np.zeros(shape=(N))
    length = path.shape[0]
    
    path[0] = underlying_path[0]
    for idx in range(1, length):
        dP = (
            reversion_strength * (underlying_path[idx - 1 + forward_shift] - path[idx - 1]) * dt + 
            volatility * np.sqrt(dt * path[idx - 1]) * Z[idx - 1]
        )
        path[idx] = abs(path[idx - 1] + dP)
    return path

def sliding_window_edge_pad(arr: npt.NDArray, window_size: int):
    pad_width = window_size // 2
    arr_padded = np.pad(arr, pad_width=pad_width, mode='edge')
    window = np.ones(window_size) / window_size
    result = np.convolve(arr_padded, window, mode='valid')
    return result