using UnityEngine;

public class LogAllAnimation : MonoBehaviour
{
	Animator animator;

	System.Collections.Generic.Dictionary<int, AnimationClip> dic_AnimHashClip;

	// Start is called before the first frame update
	void Start()
	{
		animator = GetComponent<Animator>();

		AnimationClip[] arr_AnimClips = animator.runtimeAnimatorController.animationClips;

		int len_arr_AnimClip = arr_AnimClips.Length;

		dic_AnimHashClip = new System.Collections.Generic.Dictionary<int, AnimationClip>((int)(1.5 * len_arr_AnimClip));

		for (int i = 0; i < len_arr_AnimClip; i++)
		{
			Debug.Log(arr_AnimClips[i].name);

			dic_AnimHashClip.Add(Animator.StringToHash(arr_AnimClips[i].name), arr_AnimClips[i]);
		}

		StartCoroutine(PlayAll());
	}

	// Update is called once per frame
	void Update()
	{
	}

	private System.Collections.IEnumerator PlayAll()
	{
		foreach (var clip in dic_AnimHashClip)
		{
			animator.Play(clip.Key);
			yield return new WaitForSeconds(clip.Value.length);
		}
	}

	public float GetCurrentAnimatorTime(int layer = 0)
	{
		AnimatorStateInfo animState = animator.GetCurrentAnimatorStateInfo(layer);

		int currentAnimHash = animState.fullPathHash;

		AnimationClip clip;
		dic_AnimHashClip.TryGetValue(currentAnimHash, out clip);

		float currentTime = clip.length * animState.normalizedTime;
		return currentTime;
	}

}
