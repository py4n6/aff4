package aff4.infomodel;


public interface Storable extends Queryable {
	public void add(String graph, String subject, String property, String object) ;
}
