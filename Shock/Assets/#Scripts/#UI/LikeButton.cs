using UnityEngine;
using UnityEngine.UI;

public class LikeButton : MonoBehaviour
{
	public GameObject heart;
	public float tweenTime;

	public Image burstCircle;
	public Color startColor, endColor;

	public void Tween()
	{
		LeanTween.cancel(heart);
		heart.transform.localScale = Vector3.zero;

		LeanTween.cancel(burstCircle.gameObject);
		burstCircle.transform.localScale = Vector3.zero;

		LeanTween.scale(burstCircle.gameObject, Vector3.one, tweenTime);

		LeanTween.value(burstCircle.gameObject, startColor, endColor, tweenTime)
			.setOnUpdateColor((_color) =>
			{
				burstCircle.color = _color;
			}
		);

		LeanTween.scale(heart, Vector3.one, tweenTime)
			.setEaseOutElastic()
			.setDelay(tweenTime / 2);
	}
}
