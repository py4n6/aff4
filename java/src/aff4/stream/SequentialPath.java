package aff4.stream;

public class SequentialPath implements Comparable{
	String device = null;
	long start;
	long length;
	OrderedDataStreamVisitor visitor;
	
	public SequentialPath(long start, long length, OrderedDataStreamVisitor visitor ) {
		this.visitor = visitor;
		this.start = start;
		this.length = length;
		
	}
	
	public int compareTo(Object o) {
		if (start < ((SequentialPath)o).start) {
			return -1;
		} else if (start > ((SequentialPath)o).start) {
			return 1;
		} else {
			if (length < ((SequentialPath)o).length) {
				return 1;
			} else if (length > ((SequentialPath)o).length) {
				return -1;
			} else {
				return 0;
			}
		}
	}
	
	public boolean contains(long offset) {
		if (offset >= start && offset < (start + length)) {
			return true;
		}
		return false;
	}
	
	public String toString() {
		StringBuffer sb = new StringBuffer();
		sb.append(start);
		sb.append(":");
		sb.append(length);
		return sb.toString();
	}
}

