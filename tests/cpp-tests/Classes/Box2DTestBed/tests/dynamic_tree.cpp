// MIT License

// Copyright (c) 2019 Erin Catto

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "test.h"

class DynamicTree : public Test
{
public:

	enum
	{
		e_actorCount = 128
	};

	DynamicTree()
	{
		m_worldExtent = 15.0f;
		m_proxyExtent = 0.5f;

		srand(888);

		for (int32 i = 0; i < e_actorCount; ++i)
		{
			Actor* actor = m_actors + i;
			GetRandomAABB(&actor->aabb);
			actor->proxyId = m_tree.CreateProxy(actor->aabb, actor);
		}

		m_stepCount = 0;

		float h = m_worldExtent;
		m_queryAABB.lowerBound.Set(-3.0f, -4.0f + h);
		m_queryAABB.upperBound.Set(5.0f, 6.0f + h);

		m_rayCastInput.p1.Set(-5.0, 5.0f + h);
		m_rayCastInput.p2.Set(7.0f, -4.0f + h);
		//m_rayCastInput.p1.Set(0.0f, 2.0f + h);
		//m_rayCastInput.p2.Set(0.0f, -2.0f + h);
		m_rayCastInput.maxFraction = 1.0f;

		m_automated = false;
	}

	static Test* Create()
	{
		return new DynamicTree;
	}

	void Step(Settings& settings) override
	{
		B2_NOT_USED(settings);

		m_rayActor = NULL;
		for (int32 i = 0; i < e_actorCount; ++i)
		{
			m_actors[i].fraction = 1.0f;
			m_actors[i].overlap = false;
		}

		if (m_automated == true)
		{
			int32 actionCount = b2Max(1, e_actorCount >> 2);

			for (int32 i = 0; i < actionCount; ++i)
			{
				Action();
			}
		}

		Query();
		RayCast();

		for (int32 i = 0; i < e_actorCount; ++i)
		{
			Actor* actor = m_actors + i;
			if (actor->proxyId == b2_nullNode)
				continue;

			b2Color c(0.9f, 0.9f, 0.9f);
			if (actor == m_rayActor && actor->overlap)
			{
				c.Set(0.9f, 0.6f, 0.6f);
			}
			else if (actor == m_rayActor)
			{
				c.Set(0.6f, 0.9f, 0.6f);
			}
			else if (actor->overlap)
			{
				c.Set(0.6f, 0.6f, 0.9f);
			}

			DrawAABB(&actor->aabb, c);
		}

		b2Color c(0.7f, 0.7f, 0.7f);
		DrawAABB(&m_queryAABB, c);

		g_debugDraw.DrawSegment(m_rayCastInput.p1, m_rayCastInput.p2, c);

		b2Color c1(0.2f, 0.9f, 0.2f);
		b2Color c2(0.9f, 0.2f, 0.2f);
		g_debugDraw.DrawPoint(m_rayCastInput.p1, 6.0f, c1);
		g_debugDraw.DrawPoint(m_rayCastInput.p2, 6.0f, c2);

		if (m_rayActor)
		{
			b2Color cr(0.2f, 0.2f, 0.9f);
			b2Vec2 p = m_rayCastInput.p1 + m_rayActor->fraction * (m_rayCastInput.p2 - m_rayCastInput.p1);
			g_debugDraw.DrawPoint(p, 6.0f, cr);
		}

		{
			int32 height = m_tree.GetHeight();
			DrawString(5, m_textLine, "dynamic tree height = %d", height);
			
		}

		++m_stepCount;
	}

	void Keyboard(int key) override
	{
		switch (key)
		{
		case GLFW_KEY_A:
			m_automated = !m_automated;
			break;

		case GLFW_KEY_C:
			CreateProxy();
			break;

		case GLFW_KEY_D:
			DestroyProxy();
			break;

		case GLFW_KEY_M:
			MoveProxy();
			break;
		}
	}

	bool QueryCallback(int32 proxyId)
	{
		Actor* actor = (Actor*)m_tree.GetUserData(proxyId);
		actor->overlap = b2TestOverlap(m_queryAABB, actor->aabb);
		return true;
	}

	float RayCastCallback(const b2RayCastInput& input, int32 proxyId)
	{
		Actor* actor = (Actor*)m_tree.GetUserData(proxyId);

		b2RayCastOutput output;
		bool hit = actor->aabb.RayCast(&output, input);

		if (hit)
		{
			m_rayCastOutput = output;
			m_rayActor = actor;
			m_rayActor->fraction = output.fraction;
			return output.fraction;
		}

		return input.maxFraction;
	}

private:

	struct Actor
	{
		b2AABB aabb;
		float fraction;
		bool overlap;
		int32 proxyId;
	};

	void GetRandomAABB(b2AABB* aabb)
	{
		b2Vec2 w; w.Set(2.0f * m_proxyExtent, 2.0f * m_proxyExtent);
		//aabb->lowerBound.x = -m_proxyExtent;
		//aabb->lowerBound.y = -m_proxyExtent + m_worldExtent;
		aabb->lowerBound.x = RandomFloat(-m_worldExtent, m_worldExtent);
		aabb->lowerBound.y = RandomFloat(0.0f, 2.0f * m_worldExtent);
		aabb->upperBound = aabb->lowerBound + w;
	}

	void MoveAABB(b2AABB* aabb)
	{
		b2Vec2 d;
		d.x = RandomFloat(-0.5f, 0.5f);
		d.y = RandomFloat(-0.5f, 0.5f);
		//d.x = 2.0f;
		//d.y = 0.0f;
		aabb->lowerBound += d;
		aabb->upperBound += d;

		b2Vec2 c0 = 0.5f * (aabb->lowerBound + aabb->upperBound);
		b2Vec2 min; min.Set(-m_worldExtent, 0.0f);
		b2Vec2 max; max.Set(m_worldExtent, 2.0f * m_worldExtent);
		b2Vec2 c = b2Clamp(c0, min, max);

		aabb->lowerBound += c - c0;
		aabb->upperBound += c - c0;
	}

	void CreateProxy()
	{
		for (int32 i = 0; i < e_actorCount; ++i)
		{
			int32 j = rand() % e_actorCount;
			Actor* actor = m_actors + j;
			if (actor->proxyId == b2_nullNode)
			{
				GetRandomAABB(&actor->aabb);
				actor->proxyId = m_tree.CreateProxy(actor->aabb, actor);
				return;
			}
		}
	}

	void DestroyProxy()
	{
		for (int32 i = 0; i < e_actorCount; ++i)
		{
			int32 j = rand() % e_actorCount;
			Actor* actor = m_actors + j;
			if (actor->proxyId != b2_nullNode)
			{
				m_tree.DestroyProxy(actor->proxyId);
				actor->proxyId = b2_nullNode;
				return;
			}
		}
	}

	void MoveProxy()
	{
		for (int32 i = 0; i < e_actorCount; ++i)
		{
			int32 j = rand() % e_actorCount;
			Actor* actor = m_actors + j;
			if (actor->proxyId == b2_nullNode)
			{
				continue;
			}

			b2AABB aabb0 = actor->aabb;
			MoveAABB(&actor->aabb);
			b2Vec2 displacement = actor->aabb.GetCenter() - aabb0.GetCenter();
			m_tree.MoveProxy(actor->proxyId, actor->aabb, displacement);
			return;
		}
	}

	void Action()
	{
		int32 choice = rand() % 20;

		switch (choice)
		{
		case 0:
			CreateProxy();
			break;

		case 1:
			DestroyProxy();
			break;

		default:
			MoveProxy();
		}
	}

	void Query()
	{
		m_tree.Query(this, m_queryAABB);

		for (int32 i = 0; i < e_actorCount; ++i)
		{
			if (m_actors[i].proxyId == b2_nullNode)
			{
				continue;
			}

			bool overlap = b2TestOverlap(m_queryAABB, m_actors[i].aabb);
			B2_NOT_USED(overlap);
			b2Assert(overlap == m_actors[i].overlap);
		}
	}

	void RayCast()
	{
		m_rayActor = NULL;

		b2RayCastInput input = m_rayCastInput;

		// Ray cast against the dynamic tree.
		m_tree.RayCast(this, input);

		// Brute force ray cast.
		Actor* bruteActor = NULL;
		b2RayCastOutput bruteOutput;
		for (int32 i = 0; i < e_actorCount; ++i)
		{
			if (m_actors[i].proxyId == b2_nullNode)
			{
				continue;
			}

			b2RayCastOutput output;
			bool hit = m_actors[i].aabb.RayCast(&output, input);
			if (hit)
			{
				bruteActor = m_actors + i;
				bruteOutput = output;
				input.maxFraction = output.fraction;
			}
		}

		if (bruteActor != NULL)
		{
			b2Assert(bruteOutput.fraction == m_rayCastOutput.fraction);
		}
	}

	float m_worldExtent;
	float m_proxyExtent;

	b2DynamicTree m_tree;
	b2AABB m_queryAABB;
	b2RayCastInput m_rayCastInput;
	b2RayCastOutput m_rayCastOutput;
	Actor* m_rayActor;
	Actor m_actors[e_actorCount];
	int32 m_stepCount;
	bool m_automated;
};

static int testIndex = RegisterTest("Collision", "Dynamic Tree", DynamicTree::Create);
