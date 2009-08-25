package aff4.infomodel;

import java.io.IOException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;


public class QueryTools {
	public static List<String> queryValues(Queryable volume, String g, String s, String p) throws IOException, ParseException{
		QuadList res = volume.query(g, s, p, null);
		return res.getObjects();
	}
	
	public static String queryValue(Queryable volume, String g, String s, String p) throws IOException, TooManyValuesException, ParseException{
		QuadList res = volume.query(g, s, p, null);
		if (res.size() == 1){
			return res.getObjects().get(0);
		} else if (res.size() == 0) {
			return null;
		} else {
			throw new TooManyValuesException();
		}
	}
	
	public static List<String> getUniqueGraphs(List<Quad> list) {
		Collections.sort(list);
		ArrayList<String> res = new ArrayList<String>();
		String last = null;
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
	
	public static List<String> getUniqueSubjects(List<Quad> list) {
		Collections.sort(list);
		ArrayList<String> res = new ArrayList<String>();
		String last = null;
		for (Quad s : list) {
			if (last == null) {
				res.add(s.getSubject());

			} else {
				if (s.getSubject() != last) {
					res.add(s.getSubject());
				}
			}
			last = s.getSubject();
		}
		return res;
	}
}
