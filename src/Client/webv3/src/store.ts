import { create } from "zustand";
import { immer } from "zustand/middleware/immer";

interface Layout {
  workspaceX: number;
  workspaceY: number;
  dragX: number;
  dragY: number;
  width: number;
  height: number;
  zIndex: number;
}

interface GlobalState {
  topZ: number;
  bringToFront: (id: string) => void;
  panels: Record<string, Layout>;
  createPanel: (id: string, x: number, y: number, w: number, h: number) => void;
  setPanelPosition: (id: string, x: number, y: number) => void;
  setPanelDrag: (id: string, x: number, y: number) => void;
  setPanelSize: (id: string, w: number, h: number) => void;
  removePanel: (id: string) => void;
}

export const useGlobalStore = create<GlobalState>()(
  immer((set) => ({
    topZ: 0,
    bringToFront: (id) =>
      set((state) => {
        if (state.panels[id]) {
          state.topZ += 1;
          state.panels[id].zIndex = state.topZ;
        }
      }),
    panels: {},
    createPanel: (id, x, y, w, h) =>
      set((state) => {
        if (state.panels[id] === undefined) {
          state.panels[id] = {
            workspaceX: x,
            workspaceY: y,
            dragX: x,
            dragY: y,
            width: w,
            height: h,
            zIndex: state.topZ + 1,
          };
          state.topZ += 1;
        }
      }),
    setPanelPosition: (id, x, y) =>
      set((state) => {
        if (state.panels[id]) {
          state.panels[id].workspaceX = x;
          state.panels[id].workspaceY = y;
        }
      }),
    setPanelDrag: (id, x, y) =>
      set((state) => {
        if (state.panels[id]) {
          state.panels[id].dragX = x;
          state.panels[id].dragY = y;
        }
      }),
    setPanelSize: (id, w, h) =>
      set((state) => {
        if (state.panels[id]) {
          state.panels[id].width = w;
          state.panels[id].height = h;
        }
      }),
    removePanel: (id) =>
      set((state) => {
        delete state.panels[id];
      }),
  }))
);
