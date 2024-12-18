using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;
 
public class UIRenderer3D : UIBehaviour
{
    Canvas canvas;
    //CanvasGroup canvasGroup;
    public GameObject Model;
 
    public Material CurMaterial;
    public Texture2D CurTexture;
 
    CanvasRenderer canvasRenderer;
    private Mesh mesh;
 
    public override bool IsActive()
    {
        return base.IsActive();
    }
 
    private void setCenter(Vector3[] vector3s, Vector3 center)
    {
        Vector3 pos;
        for (int i = 0; i < vector3s.Length; i++)
        {
            pos = vector3s[i] - center;
            vector3s[i] = pos;
        }
    } 
 
    private Mesh createMesh()
    {
       Mesh createMesh = new Mesh();
        Vector3[] vertices = new Vector3[]
        {
            new Vector3(0,0,0),
            new Vector3(0,100,0),
            new Vector3(100,100,0),
            new Vector3(100,0,0),
        };
        setCenter(vertices, new Vector3(50, 50, 50));
 
        List<UIVertex> uIVertices = new List<UIVertex>
        {
            new UIVertex{ position= vertices[0],color =new Color(1,1,1,1),uv0=new Vector2(0,0)},
            new UIVertex{ position= vertices[1],color =new Color(1,1,1,1),uv0=new Vector2(0,1)},
            new UIVertex{ position= vertices[2],color =new Color(1,1,1,1),uv0=new Vector2(1,1)},
            new UIVertex{ position= vertices[3],color =new Color(1,1,1,1),uv0=new Vector2(1,0)},
        };
        List<int> indexs = new List<int>
        {
            0,1,2,
            0,2,3
        };

        VertexHelper vertexHelper = new VertexHelper();
        vertexHelper.AddUIVertexStream(uIVertices, indexs);
        vertexHelper.FillMesh(createMesh);
 
        createMesh.RecalculateBounds();
 
        return createMesh;
    }
 
    private void setRenderer(Mesh mesh)
    {
        if (!mesh)
        {
            return ;
        }
        canvasRenderer = GetComponent<CanvasRenderer>();
        if (!canvasRenderer)
        {
            return;
        }
        canvasRenderer.Clear();
        canvasRenderer.cullTransparentMesh = false;
        canvasRenderer.cull = false;
        canvasRenderer.SetMesh(mesh);
        canvasRenderer.SetMaterial(CurMaterial, CurTexture);
        canvasRenderer.SetColor(new Color(1, 1, 1, 1));
    }
 
 
    private Mesh getModelMesh()
    {
        if (!Model)
        {
            return null;
        }
        MeshFilter  meshFilter = Model.GetComponent<MeshFilter>();
 
        if (!meshFilter)
        {
            return null;
        }
 
        Mesh newMesh =  meshFilter.sharedMesh;
 
        Vector3[] vers = newMesh.vertices;
        for (int i = 0; i < vers.Length; i++)
        {
            vers[i] = vers[i] * 100;
        }
 
        Mesh mesh = new Mesh();
       
        mesh.SetVertices(vers);
        mesh.SetTriangles(newMesh.triangles, 0);
        mesh.SetUVs(0, newMesh.uv);
        mesh.SetNormals(newMesh.normals);
        mesh.SetTangents(newMesh.tangents);
        mesh.SetColors(mesh.colors);
 
        return mesh;
    }
    private void createRenderer()
    {
       //mesh = createMesh();
        mesh = getModelMesh();
 
        setRenderer(mesh);
    }
 
    protected override void Awake()
    {
        createRenderer();
        base.Awake();
    }  
}
