package aff4.infomodel;

import java.io.IOException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


public class QueryTools {
	public static List<Node> queryValues(Queryable volume, Node g, Node s, Node p) throws IOException, ParseException{
		QuadList res = volume.query(g, s, p, null);
		return res.getObjects();
	}
	
	public static Node queryValue(Queryable volume, Node g, Node s, Node p) throws IOException, TooManyValuesException, ParseException{
		QuadList res = volume.query(g, s, p, null);
		if (res.size() == 1){
			return res.getObjects().get(0);
		} else if (res.size() == 0) {
			return null;
		} else {
			throw new TooManyValuesException();
		}
	}
	
	public static Set<Resource> getUniqueGraphs(Set<Quad> list) {
		ArrayList<Quad> l = new ArrayList<Quad>(list);
		return getUniqueGraphs(l);
	}
	
	public static Set<Resource> getUniqueGraphs(List<Quad> list) {
		Collections.sort(list);
		HashSet<Resource> res = new HashSet<Resource>();
		Resource last = null;
		for (Quad s : list) {
			if (last == null) {
				res.add(s.getGraph());
			} else {
				if (!s.getGraph().equals(last)) {
					res.add(s.getGraph());
				}
			}
			last = s.getGraph();
		}
		return res;
	}
	
	public static List<Resource> getUniqueSubjects(List<Quad> list) {
		Collections.sort(list);
		ArrayList<Resource> res = new ArrayList<Resource>();
		Resource last = null;
		for (Quad s : list) {
			if (last == null) {
				res.add(s.getSubject());

			} else {
				if (!last.equals(s.getSubject())) {
					res.add(s.getSubject());
				}
			}
			last = s.getSubject();
		}
		Collections.sort(res);
		return res;
	}
}
