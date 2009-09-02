package aff4.infomodel;


public interface Storable extends Queryable {
	public void add(Node graph, Node subject, Node property, Node object) ;
}
