import clsx from "clsx";
import { useState, useImperativeHandle, forwardRef, useEffect, useCallback } from "react";

type IntegerInputProps = {
  isDisabled: boolean;
  min: number;
  max: number;
};

export type IntegerInputRef = {
  getValue: () => number | null;
};

const IntegerInput = forwardRef<IntegerInputRef, IntegerInputProps>(
  ({ isDisabled, min, max }, ref) => {
    const [value, setValue] = useState("");
    const [error, setError] = useState<string | null>(null);

    const validate = useCallback(
      (val: string): string | null => {
        if (val === "") {
          return `Value required between ${min} and ${max}`;
        }

        if (!/^\d+$/.test(val)) {
          return "Only whole numbers are allowed";
        }

        const num = parseInt(val);
        if (num < min || num > max) {
          return `Must be between ${min} and ${max}`;
        }
        return null;
      },
      [max, min]
    );

    useEffect(() => {
      if (isDisabled) {
        setValue("");
        setError(null);
      } else {
        const r = validate(value);
        setError(r);
      }
    }, [min, max, isDisabled, value, validate]);

    useImperativeHandle(ref, () => ({
      getValue: () => {
        if (!isDisabled && validate(value) === null) {
          return parseInt(value);
        } else {
          return null;
        }
      },
    }));

    return (
      <input
        type='number'
        value={value}
        onChange={(e) => setValue(e.target.value)}
        className={clsx(
          "outline-1 w-14",
          isDisabled && "bg-gray-300 text-gray-600 cursor-not-allowed",
          error && "outline-2 outline-red-600"
        )}
        step={1}
        min={min}
        title={error || undefined}
        disabled={isDisabled}
      />
    );
  }
);

export default IntegerInput;
