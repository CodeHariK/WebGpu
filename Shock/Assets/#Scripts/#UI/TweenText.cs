using TMPro;
using UnityEngine;

public class TweenText : MonoBehaviour
{
	public float tweenTime;
	public TextMeshProUGUI textMesh;

	void Start()
	{
		LeanTween.reset();
		Tween();
	}

	void Update()
	{

	}

	public void Tween()
	{
		LeanTween.cancel(gameObject);

		transform.localScale = Vector3.one;

		LeanTween.scale(gameObject, Vector3.one * 2, tweenTime)
			.setEasePunch();

		LeanTween.value(gameObject, 0, 1, tweenTime)
			.setEaseInBounce()
			.setOnUpdate(setText);
	}

	public void setText(float value)
	{
		textMesh.text = value.ToString("F1");
	}
}
