import clsx from "clsx";
import type { ReactElement } from "react";

const flashGreenClass = "bg-green-300";
const flashRedClass = "bg-red-300";
const flashGreenText = "text-green-300";
const flashRedText = "text-red-300";
interface FlashBackgroundProps {
  current: number | null;
  previous: number | null;
  isUpGreen?: boolean;
  emptyState?: string | ReactElement;
}
export function FlashBackground({
  current,
  previous,
  isUpGreen = true,
  emptyState = "--",
}: FlashBackgroundProps) {
  if (current !== null && previous !== null) {
    const delta = current - previous;
    let colorClass = null;
    if (delta > 0 && isUpGreen) {
      colorClass = flashGreenClass;
    } else if (delta > 0 && !isUpGreen) {
      colorClass = flashRedClass;
    } else if (delta < 0 && isUpGreen) {
      colorClass = flashRedClass;
    } else if (delta < 0 && !isUpGreen) {
      colorClass = flashGreenClass;
    }
    return (
      <div className={clsx(colorClass, "text-center", "align-middle")}>
        <span>{current.toFixed(2)}</span>
      </div>
    );
  } else if (current !== null && previous === null) {
    return (
      <div className='text-center align-middle'>
        <span>{current.toFixed(2)}</span>
      </div>
    );
  } else if (current === null && previous !== null) {
    return (
      <div>
        <span>{emptyState}</span>
      </div>
    );
  } else {
    // Both are null
    return (
      <div>
        <span>{emptyState}</span>
      </div>
    );
  }
}

interface FlashDeltaProps {
  value: number | null;
  delta: number | null;
  isUpGreen?: boolean;
  emptyState?: string | ReactElement;
}
export function FlashDelta({ value, delta, isUpGreen, emptyState }: FlashDeltaProps) {
  if (value !== null && delta !== null) {
    let colorClass = null;
    if (delta > 0 && isUpGreen) {
      colorClass = flashGreenText;
    } else if (delta < 0 && isUpGreen) {
      colorClass = flashRedText;
    } else if (delta > 0 && !isUpGreen) {
      colorClass = flashRedText;
    } else if (delta < 0 && !isUpGreen) {
      colorClass = flashGreenText;
    }
    if (delta !== 0) {
      return (
        <div className={clsx(colorClass, "text-center", "align-middle")}>
          <span>
            {value.toFixed(2)} ({delta > 0 ? "+" : ""}
            {delta.toFixed(2)})
          </span>
        </div>
      );
    } else {
      return (
        <div className={clsx(colorClass, "text-center", "align-middle")}>
          <span>{value.toFixed(2)}</span>
        </div>
      );
    }
  } else if (value !== null && delta === null) {
    return (
      <div className='text-center'>
        <span>{value.toFixed(2)}</span>
      </div>
    );
  } else if (value === null && delta !== null) {
    if (delta !== 0) {
      let colorClass = null;
      if (delta > 0 && isUpGreen) {
        colorClass = flashGreenClass;
      } else if (delta < 0 && isUpGreen) {
        colorClass = flashRedClass;
      } else if (delta > 0 && !isUpGreen) {
        colorClass = flashRedClass;
      } else if (delta < 0 && !isUpGreen) {
        colorClass = flashGreenClass;
      }
      return (
        <div className={clsx(colorClass, "text-center", "align-middle")}>
          <span className=''>
            ({delta > 0 ? "+" : ""}
            {delta.toFixed(2)})
          </span>
        </div>
      );
    } else {
      return (
        <div>
          <span>{emptyState}</span>
        </div>
      );
    }
  } else {
    // Both are null
    return (
      <div>
        <span>{emptyState}</span>
      </div>
    );
  }
}
