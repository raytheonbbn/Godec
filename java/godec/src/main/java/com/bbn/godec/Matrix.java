package com.bbn.godec;

public class Matrix {
  private Vector[] cols;

  public Matrix(int numRows, int numCols) {
    cols = new Vector[numCols];
    for(int colIdx = 0; colIdx < numCols; colIdx++) {
      cols[colIdx] = new Vector(numRows);
    }
  }

  public Matrix(Vector[] _cols) {
    cols = _cols;
  }

  public int cols() {return cols.length;}
  public int rows() {return cols[0].size();}

  public boolean compareTo(Matrix m) {
    if (cols() != m.cols() || rows() != m.rows()) return false;
    for(int rowIdx = 0; rowIdx < cols[0].size(); rowIdx++) {
      for(int colIdx = 0; colIdx < cols.length; colIdx++) {
        if (get(rowIdx, colIdx) != m.get(rowIdx, colIdx)) return false;
      }
    }
    return true;
  }

  public void conservativeResize(int newNumRows, int newNumCols) {
    Vector[] newCols = new Vector[newNumCols];
    System.arraycopy(cols, 0, newCols, 0, Math.min(cols.length, newNumCols));
    for(int idx = Math.min(cols.length, newNumCols); idx < newNumCols; idx++) {
      newCols[idx] = new Vector(newNumRows);
    }
    for(int idx = 0; idx < newNumCols; idx++) {
      newCols[idx].conservativeResize(newNumRows);
    }
    cols = newCols;
  }

  public void addColumns(Matrix addCols) {
    Vector[] newCols = new Vector[cols.length+addCols.cols.length];
    System.arraycopy(cols, 0, newCols, 0, cols.length);
    System.arraycopy(addCols.cols, 0, newCols, cols.length, addCols.cols.length);
    cols = newCols;
  }

  public float get(int row, int col) {
    return cols[col].mData[row];
  }
  public void set(int row, int col, float val) {
    cols[col].mData[row] = val;
  }

  @Override
    public String toString() {
      String out = "";
      for(int rowIdx = 0; rowIdx < cols[0].size(); rowIdx++) {
        for(int colIdx = 0; colIdx < cols.length; colIdx++) {
          out += get(rowIdx, colIdx)+" ";
        }
        out += "\n";
      }
      return out;
    }
}
