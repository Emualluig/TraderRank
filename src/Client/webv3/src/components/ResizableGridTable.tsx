import React, { useEffect, useRef, useState, type ReactElement } from "react";

const MIN_COL_WIDTH = 20; // hard floor for every column in px

interface ResizableGridTableProps {
  headers: (ReactElement | string)[];
  data: (ReactElement | string)[][];
}

function ResizableGridTable({ headers, data }: ResizableGridTableProps) {
  const [colWidths, setColWidths] = useState<number[]>(headers.map(() => MIN_COL_WIDTH));
  const containerRef = useRef<HTMLDivElement | null>(null);

  // resize algorithm for a single drag
  const resizeBetween = (
    widths: number[],
    handleIdx: number, // gutter is between handleIdx & handleIdx+1
    delta: number // +right, -left (mouse movement)
  ) => {
    const next = [...widths];

    if (delta === 0) return next;

    if (delta > 0) {
      /* drag ➡ right: grow left col, shrink right‐side neighbours */
      let need = delta;
      let j = handleIdx + 1; // start with immediate right
      while (need && j < next.length) {
        const give = Math.min(next[j] - MIN_COL_WIDTH, need);
        next[j] -= give;
        need -= give;
        j += 1;
      }
      const grown = delta - need; // how much we actually “found”
      next[handleIdx] += grown;
    } else {
      /* drag ⬅ left: grow right col, shrink left‐side neighbours */
      let need = -delta;
      let j = handleIdx;
      while (need && j >= 0) {
        const give = Math.min(next[j] - MIN_COL_WIDTH, need);
        next[j] -= give;
        need -= give;
        j -= 1;
      }
      const grown = -delta - need;
      next[handleIdx + 1] += grown;
    }

    return next;
  };

  const startDrag = (handleIdx: number, e: React.MouseEvent) => {
    e.preventDefault();
    const startX = e.clientX;
    const startWidths = colWidths;

    const onMove = (ev: MouseEvent) => {
      const delta = ev.clientX - startX;
      setColWidths(resizeBetween(startWidths, handleIdx, delta));
    };

    const onUp = () => {
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    };

    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  };

  // ResizeObserver - keep grid exactly as wide as the container
  useEffect(() => {
    if (!containerRef.current) return;
    const ro = new ResizeObserver(([entry]) => {
      const full = entry.contentRect.width;
      setColWidths((prev) => {
        let widths = [...prev];
        const currentTotal = widths.reduce((s, w) => s + w, 0);
        let diff = full - currentTotal;

        if (Math.abs(diff) < 0.5) return prev; // nothing to do

        /* distribute +/- diff evenly, honouring MIN_COL_WIDTH on shrink */
        const adjustable = widths.map((w) => (diff < 0 && w <= MIN_COL_WIDTH ? 0 : 1));
        let slots = adjustable.reduce<number>((s, v) => s + v, 0);
        if (!slots) return prev;

        while (Math.abs(diff) > 0.5 && slots) {
          const slice = diff / slots;
          widths = widths.map((w, i) => {
            if (!adjustable[i]) return w;
            const candidate = w + slice;
            if (candidate < MIN_COL_WIDTH) {
              diff += w - MIN_COL_WIDTH; // still need to shrink more
              adjustable[i] = 0;
              return MIN_COL_WIDTH;
            }
            return candidate;
          });
          diff = full - widths.reduce((s, w) => s + w, 0);
          slots = adjustable.reduce<number>((s, v) => s + v, 0);
        }
        return widths;
      });
    });

    ro.observe(containerRef.current);
    return () => ro.disconnect();
  }, []);

  const template = colWidths.map((w) => `${w}px`).join(" ");

  return (
    <div
      ref={containerRef}
      className='w-full border shadow-sm rounded overflow-y-auto overflow-x-hidden'
    >
      <div
        style={{ minWidth: `${colWidths.reduce((s, w) => s + w, 0)}px` }}
        className='divide-y-1 divide-black'
      >
        <div
          className='grid bg-gray-100 divide-x-1 divide-black sticky top-0'
          style={{ gridTemplateColumns: template }}
        >
          {headers.map((label, i) => (
            <div key={i} className='relative px-2 py-2 bg-gray-100 truncate'>
              {typeof label === "string" ? <span className='font-medium'>{label}</span> : label}
              {i < headers.length - 1 && (
                <div
                  onMouseDown={(e) => startDrag(i, e)}
                  className='absolute top-0 right-0 h-full w-2 cursor-col-resize z-10 hover:bg-gray-300'
                />
              )}
            </div>
          ))}
        </div>

        {data.map((row, r) => (
          <div
            key={r}
            className='grid divide-x-1 divide-black'
            style={{ gridTemplateColumns: template }}
          >
            {row.map((cell, c) => (
              <div key={c} className='px-2 py-2 bg-white truncate'>
                {cell}
              </div>
            ))}
          </div>
        ))}
      </div>
    </div>
  );
}

export default ResizableGridTable;
